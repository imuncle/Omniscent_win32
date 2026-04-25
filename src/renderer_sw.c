#include "renderer_sw.h"
#include "renderer_common.h"
#include "model.h"
#include "texture.h"
#include "lightmap.h"
#include "palette.h"
#include "util.h"

static HDC g_hdc_mem = NULL;
static HBITMAP g_h_dib = NULL;
static uint8_t *g_dib_bits = NULL;
static HDC g_hdc = NULL;

static void sw_convert(const uint8_t *src, int y0, int y1)
{
    int p = y0 * 960;
    for (int y = y0; y <= y1; y++)
    {
        for (int x = 0; x < 320; x++, p += 3)
        {
            int idx = src[y * 320 + x];
            g_dib_bits[p + 0] = g_palette[idx * 3 + 2];
            g_dib_bits[p + 1] = g_palette[idx * 3 + 1];
            g_dib_bits[p + 2] = g_palette[idx * 3 + 0];
        }
    }
}

int sw_renderer_init(HWND hwnd)
{
    g_hdc = GetDC(hwnd);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = FB_W;
    bmi.bmiHeader.biHeight = -FB_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_h_dib = CreateDIBSection(g_hdc, &bmi, DIB_RGB_COLORS, (void **)&g_dib_bits, NULL, 0);
    if (!g_h_dib)
    {
        ReleaseDC(NULL, g_hdc);
        return 0;
    }

    g_hdc_mem = CreateCompatibleDC(g_hdc);
    SelectObject(g_hdc_mem, g_h_dib);
    sw_convert(g_bg, 0, 199);
    return 1;
}

static const uint8_t UV[8] = {0, 63, 0, 0, 63, 0, 63, 63}; /* 四角的UV坐标 */
#define my_roundf(x) ((int)((x) + ((x) >= 0 ? 0.5f : -0.5f)))
float v[4][6];
float poly_int[6][5];
float cv[5][6];
/* 边的属性存储：t0 = 当前值, t1 = 每行步长 */
/* t0[0-4] = 左边界属性, t0[5-9] = 右边界属性 */
/* 属性: [0]=X, [1]=U*iz, [2]=V*iz, [3]=iz(1/z), [4]=雾化值 */
float t0[10], t1[10], t2[5], t3[5];
int dir[2] = {-1, 1};   /* 逆时针/顺时针 */
int offset[2] = {0, 5}; /* t0数组偏移：左=0, 右=5 */
void sw_draw_quad(int qi)
{
    for (int i = 0; i < 4; i++)
    {
        int vi = g_quads[qi].idx[i];
        v[i][0] = g_tvtxs[vi * 3 + 0];
        v[i][1] = g_tvtxs[vi * 3 + 1];
        v[i][2] = g_tvtxs[vi * 3 + 2];
        /* 计算雾化值：基于深度 Z 和顶点光照值 */
        int z = (int)my_roundf(v[i][2]) + 512;
        if (z < 0)
            return; /* 完全在相机后面 → 丢弃 */
        if (z > 511)
            z = 511;                                           /* 限制最大深度 */
        v[i][3] = (float)(UV[i * 2] << 16);                    /* U 坐标（固定点） */
        v[i][4] = (float)(UV[i * 2 + 1] << 16);                /* V 坐标（固定点） */
        v[i][5] = (float)(((z << 7) * g_vtxs[vi].v[3]) >> 16); /* 雾化值 = depth × light */
    }
    int vc = 0;
    int neg = v[3][2] < 0;
    for (int i = 0; i < 4; i++)
    {
        float z = v[i][2];
        if ((z < 0) != neg)
        { /* 穿越裁剪面 → 计算交点 */
            int i0 = (i == 0) ? 3 : i - 1;
            float t = z / (z - v[i0][2]); /* 交点参数 t ∈ [0,1] */
            for (int k = 0; k < 6; k++)
                cv[vc][k] = (v[i0][k] - v[i][k]) * t + v[i][k];
            vc++;
            neg = !neg;
        }
        if (z < 0)
        {
            memcpy(cv[vc], v[i], 24);
            vc++;
        }
    }
    if (vc < 2)
        return; /* 裁剪后没有有效顶点 */

    /* 步骤6：投影变换 + 透视校正参数计算 */
    int min_x = 320, max_x = 0, min_y = 180, max_y = 0, index_l, index_r;
    for (int i = 0; i < vc; i++)
    {
        float z = (13.0f - cv[i][2]) / 160.0f; /* 映射后的深度值 */
        float iz = 1.0f / z;                   /* 深度倒数（用于透视校正） */
        float x = (cv[i][0] * 1.2f * iz + 160.0f);
        float y = (cv[i][1] * iz + 100.0f);
        poly_int[0][i] = y;               /* 屏幕 Y */
        poly_int[1][i] = x;               /* 屏幕 X */
        poly_int[2][i] = (cv[i][3] * iz); /* U*iz */
        poly_int[3][i] = (cv[i][4] * iz); /* V*iz */
        poly_int[4][i] = (256.0f * iz);   /* iz (1/z) */
        poly_int[5][i] = (cv[i][5]);      /* 雾化值 */
        if (x < min_x)
            min_x = x;
        if (x > max_x)
            max_x = x;
        if (y < min_y)
        {
            min_y = y;
            index_l = index_r = i;
        } /* 最高点作为起始边界点 */
        if (y > max_y)
            max_y = y;
    }
    if (max_x < 0 || min_x > 319 || max_y < 21 || min_y > 179)
        return;
    if (max_y > 179)
        max_y = 179;
    if (min_y == max_y)
        return; /* 退化为水平线 */
    uint8_t *texture = g_textures[g_quads[qi].idx[4]];
    float *sy = poly_int[0], *sx = poly_int[1];

    int scan_y = min_y;
    memset(t2, 0, 5);
    memset(t3, 0, 5);

    /* 阶段2：逐行扫描填充 */
    while (scan_y <= max_y)
    {
        /* 阶段2a：更新左右边界——沿多边形边缘前进到当前扫描行的有效边 */
        int *idx_ptr[2] = {&index_l, &index_r};
        for (int side = 0; side < 2; side++)
        {
            /* 跳过已经处理过的顶点（当前扫描行已过该顶点） */
            while (scan_y == (short)sy[*idx_ptr[side]])
            {
                int i0 = *idx_ptr[side];
                int i1 = i0 + dir[side];
                if (i1 < 0)
                    i1 = vc - 1;
                else if (i1 >= vc)
                    i1 = 0;
                *idx_ptr[side] = i1;
                /* 计算新边的属性变化率 */
                int dy = (short)sy[i1] - scan_y;
                if (dy > 0)
                {
                    for (int j = 0; j < 5; j++)
                    {
                        t0[offset[side] + j] = poly_int[j + 1][i0];                              /* 当前值 = 顶点属性 */
                        t1[offset[side] + j] = (poly_int[j + 1][i1] - poly_int[j + 1][i0]) / dy; /* 每行步长 */
                    }
                }
            }
        }

        /* 阶段2b-2d：绘制当前扫描线（在可见区域内） */
        if (scan_y >= 21)
        {
            int l = t0[0], r = t0[5], dx = r - l;
            if (dx > 0 && r > 0 && l < 320)
            {
                int start_x = l < 0 ? 0 : l;
                int end_x = r > 320 ? 320 : r;
                int dl = start_x - l; /* 起始偏移量 */

                /* 计算扫描线方向的属性步长（per-pixel） */
                /* t2 = 每像素属性增量, t3 = 当前像素的属性值 */
                for (int i = 1; i < 5; i++)
                {
                    t2[i] = (t0[5 + i] - t0[i]) / dx;
                    t3[i] = t2[i] * dl + t0[i];
                }

                int p = scan_y * 320 + start_x, x = start_x;
                while (x < end_x)
                {
                    /* 以 16 像素为单位分块渲染，每块重新计算透视校正的 UV */
                    int step = end_x - x;
                    if (step > 16)
                        step = 16;

                    /* 透视校正：U = (U*iz) / iz * 256, V = (V*iz) / iz * 256 */
                    uint16_t u0 = (uint16_t)((float)t3[1] / t3[3]);
                    uint16_t v0 = (uint16_t)((float)t3[2] / t3[3]);
                    /* 预计算块末端的 UV（用于线性插值） */
                    t3[1] += t2[1] * step;
                    t3[2] += t2[2] * step;
                    t3[3] += t2[3] * step;
                    uint16_t u1 = (uint16_t)((float)t3[1] / t3[3]);
                    uint16_t v1 = (uint16_t)((float)t3[2] / t3[3]);
                    /* 块内线性 UV 步长 */
                    int du = (short)(u1 - u0) / step, dv = (short)(v1 - v0) / step;

                    /* 逐像素填充 */
                    for (int i = 0; i < step; i++)
                    {
                        /* 计算纹理坐标：UV 都是 16-bit 固定点，高8位是整数部分 */
                        int q = (((v0 >> 8) & 0x3F) << 6) | ((u0 >> 8) & 0x3F); /* 映射到 64x64 纹理 */
                        uint8_t texel = texture[q];
                        if (texel)
                        {
                            /* 应用光照和雾化：查 lightmap[texel + 雾化偏移]
                               t3[4] 是雾化值，高字节 & 0x7F00 作为光照级别偏移
                               距离越远，雾化值越小 → 光照级别越低 → 颜色越暗 */
                            g_fb[p] = g_lightmap[texel + ((int32_t)t3[4] & 0x7F00)];
                        }
                        t3[4] += (short)t2[4]; /* 更新雾化值 */
                        u0 += (short)du;
                        v0 += (short)dv; /* 线性步进 UV */
                        p++;
                        x++;
                    }
                }
            }
        }

        /* 更新边界属性到下一行 */
        for (int i = 0; i < 10; i++)
            t0[i] += t1[i];
        scan_y++;
    }
}

void sw_display(void)
{
    sw_convert(g_fb, 18, 179);
    StretchBlt(g_hdc, 0, 0, 960, 600, g_hdc_mem, 0, 0, FB_W, FB_H, SRCCOPY);
}

void sw_renderer_cleanup(void)
{
    if (g_hdc_mem)
        DeleteDC(g_hdc_mem);
    if (g_h_dib)
        DeleteObject(g_h_dib);
    if (g_hdc)
        ReleaseDC(NULL, g_hdc);
}
