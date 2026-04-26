#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#if defined(_MSC_VER)
int _fltused = 0;
__declspec(naked) void _ftol2(void){
    __asm fistp dword ptr[esp - 4] __asm mov eax, [esp - 4] __asm ret}
#endif
#define FB_W 320
#define FB_H 200
#define FB_SIZE (FB_W * FB_H)
#define TEX_COUNT 16
#define TEX_SIZE (64 * 64)
#define PAL_SIZE (3 * 256)
#define LM_SIZE (128 * 256)
#define MAX_VTX 362
#define MAX_QUAD 367
#define MIDI_MAX_CH 4
uint8_t g_fb[FB_SIZE];
uint8_t g_bg[FB_SIZE];
int16_t g_sort_list[(MAX_QUAD + 1) * 2];
uint8_t g_textures[TEX_COUNT][TEX_SIZE];
int16_t g_door_counter = -4250;
uint8_t g_palette[PAL_SIZE];
uint8_t g_lightmap[LM_SIZE];
int g_vtx_count = 0;
int g_quad_count = 0;
typedef struct
{
    int16_t v[4];
} model_vertex_t;
typedef struct
{
    int16_t idx[5];
} model_quad_t;
model_vertex_t g_vtxs[MAX_VTX];
model_quad_t g_quads[MAX_QUAD];
float g_tvtxs[MAX_VTX * 3];
#pragma function(memset)
__declspec(naked) void *memset(void *dst, int val, size_t count)
{
    __asm { push edi }
    __asm { mov edi, [esp + 8] }
    __asm { mov eax,[esp+12] }
    __asm { mov ecx, [esp + 16] }
    __asm { rep stosb }
    __asm { mov eax, [esp + 8] }
    __asm { pop edi }
    __asm { ret }
}
static uint32_t rnd_b = 0x000010FB;
int rnd_next(int x)
{
    rnd_b = rnd_b * 0x08088405 + 1;
    return (int)((((rnd_b >> 16) & 0xFFFF) * x) >> 16) & 0xFFFF;
}
void rotate_vec2(float *v1, float *v2, float angle_rad)
{
    float _s, _o, _t1, _t2;
    __asm { fld angle_rad }
    __asm { fsincos }
    __asm { fstp _o }
    __asm {fstp _s} _t1 = *v1;
    _t2 = *v2;
    *v1 = _o * _t1 - _s * _t2;
    *v2 = _s * _t1 + _o * _t2;
}
static float int16_to_rad = 4.79368996e-5f;
void rotate_vec3(float v[3], int16_t angles_int16[3])
{
    float _a2 = (float)angles_int16[2] * int16_to_rad;
    float _a1 = (float)angles_int16[1] * int16_to_rad;
    float _a0 = (float)angles_int16[0] * int16_to_rad;
    rotate_vec2(&v[1], &v[2], _a2); /* Step 1: y-z 平面 */
    rotate_vec2(&v[2], &v[0], _a1); /* Step 2: z-x 平面 */
    rotate_vec2(&v[0], &v[1], _a0); /* Step 3: x-y 平面 */
}
/* ============================================================
   排序：对 (int16_t, int16_t) 对的数组进行快速排序
   ============================================================
   data 数组存储的是成对的 int16_t 值：(key, index)。
   按 key 降序排序（从大到小），用于四边形深度排序——
   Z 值越大表示离相机越近（因为在 view space 中相机朝向 +Z，
   物体 Z 坐标越大越靠近相机），所以按 Z 降序意味着从远到近绘制，
   这正是 Painter's Algorithm 的要求。
   ============================================================ */
static void qsort_pairs(int16_t data[], int l, int r)
{
    if (l >= r)
        return;
    int ll = l, rr = r;
    int16_t x = data[ll * 2]; /* 选取最左元素作为 pivot */
    do
    {
        while (data[ll * 2] > x)
            ll++; /* 左边找 <= pivot 的 */
        while (data[rr * 2] < x)
            rr--; /* 右边找 >= pivot 的 */
        if (ll > rr)
            break;
        int16_t t0 = data[ll * 2], t1 = data[ll * 2 + 1];
        data[ll * 2] = data[rr * 2];
        data[ll * 2 + 1] = data[rr * 2 + 1];
        data[rr * 2] = t0;
        data[rr * 2 + 1] = t1;
        ll++;
        rr--;
    } while (ll <= rr);
    qsort_pairs(data, l, rr);
    qsort_pairs(data, ll, r);
}
static const uint8_t STAR_PATTERN[25] = {
    0x00,0x00,0x03,0x00,0x00, 0x00,0x02,0x05,0x02,0x00,
    0x03,0x05,0x07,0x05,0x03, 0x00,0x02,0x05,0x02,0x00, 0x00,0x00,0x03,0x00,0x00
};
void draw_star(uint8_t buf[], int offset, int stride, int color, int value)
{
    int index = offset, p = 0;
    for (int i = 0; i < 5; i++, index += stride)
        for (int j = 0; j < 5; j++, p++, index++)
        {
            uint8_t x = STAR_PATTERN[p];
            if (x > value)
                buf[index] = (uint8_t)(((x - value) + (color >> 8)) << (color & 0xFF));
        }
}
static const uint8_t PALETTE_TABLE[] = {
    0x1F,0x3F,0x3F,0x3F, 0x01,0x00,0x00,0x00,
    0x1F,0x28,0x20,0x3F, 0x01,0x00,0x00,0x00,
    0x1F,0x3F,0x00,0x00, 0x01,0x06,0x01,0x00,
    0x1F,0x3F,0x29,0x14, 0x01,0x00,0x00,0x00,
    0x1F,0x3F,0x3F,0x08, 0x01,0x00,0x00,0x00,
    0x1F,0x38,0x38,0x3F, 0x01,0x3F,0x00,0x00,
    0x10,0x3F,0x3F,0x00, 0x10,0x3F,0x00,0x00, 
    0x01,0x16,0x05,0x00, 0x07,0x3F,0x38,0x11,
};
void palette_generate(void)
{
    for (int i = 0; i < 3; i++)
    {
        int a = 0, b = 0, p = 0, q = 0;
        for (int j = 0; j < 0x10; j++)
        {
            a = (a & 0xFF) | (PALETTE_TABLE[p + i + 1] << 8);
            a = ((a - b) / PALETTE_TABLE[p]);
            for (int k = 0; k < PALETTE_TABLE[p]; k++, q++)
            {
                b += a;
                g_palette[q * 3 + i] = (uint8_t)(b >> 8);
            }
            p += 4;
        }
    }
    for (int i = 0; i < PAL_SIZE; i++)
        g_palette[i] = (uint8_t)((g_palette[i] << 2) | (g_palette[i] >> 4));
    int r = 0;
    for (int i = 0; i < 0x80; i++)
    {
        for (int j = 0; j < 0xC0; j++)
            g_lightmap[r++] = (uint8_t)((j & 0xE0) + (((j & 0x1F) * i * 2) >> 8));
        for (int j = 0xC0; j < 0x100; j++)
            g_lightmap[r++] = (uint8_t)j;
    }
}
static int tex01_data[0x1E][2];
static const uint8_t TEXTURE_PATTERN_TABLE[] = {
    0x13,0x0F,0x0F,0x31,0x31,0x0A, 0x13,0x10,0x10,0x30,0x30,0x11,
    0x13,0x11,0x11,0x2F,0x2F,0x18, 0x13,0x12,0x12,0x2E,0x2E,0xBE,
    0x13,0x15,0x15,0x2B,0x2B,0x1E, 0x25,0x00,0x00,0x3E,0x1E,0x03,
    0x25,0x03,0x03,0x3E,0x1E,0x07, 0x25,0x03,0x03,0x3B,0x1B,0xFC,
    0x25,0x00,0x20,0x3E,0x3E,0x03, 0x25,0x03,0x23,0x3E,0x3E,0x07,
    0x25,0x03,0x23,0x3B,0x3B,0xFC, 0x25,0x00,0x00,0x3F,0x3F,0xFE,
    0x2C,0x00,0x0A,0x3F,0x0F,0xF6, 0x2C,0x00,0x0B,0x3F,0x10,0x04,
    0x2C,0x00,0x2F,0x3F,0x34,0xF6, 0x2C,0x00,0x30,0x3F,0x35,0x04,
    0x2E,0x13,0x18,0x2C,0x27,0x05, 0x2E,0x14,0x19,0x2C,0x27,0xF4,
    0x2E,0x14,0x19,0x2B,0x26,0x07, 0x3E,0x14,0x19,0x2B,0x26,0x06,
    0x2F,0x00,0x18,0x3F,0x1E,0x06, 0x2F,0x00,0x19,0x3F,0x1F,0xFA,
    0x3F,0x00,0x19,0x3F,0x1E,0x06, 0x2F,0x00,0x20,0x3F,0x26,0x06,
    0x2F,0x00,0x21,0x3F,0x27,0xFA, 0x3F,0x00,0x21,0x3F,0x26,0x06,
};
static const uint8_t TEXTURE_BACKGROUND_TABLE[] = {
    0x22,0xC0, 0x26,0x01, 0x26,0x01, 0x26,0x00,
    0x20,0x00, 0x20,0x20, 0x20,0x40, 0x22,0x60,
    0x24,0x00, 0x22,0x60, 0x24,0x00, 0x24,0x00,
    0x24,0x00, 0x24,0x00,
};
void generate_circle_noise(int id, int r, int n)
{
    uint8_t *tex = g_textures[id];
    memset(tex, 0, TEX_SIZE);
    int r2 = r * r;
    while (n--)
    {
        int p = rnd_next(0x1000);
        for (int y = -r; y < r; y++)
        {
            for (int x = -r; x < r; x++)
            {
                if (x * x + y * y <= r2)
                {
                    tex[(p + y * 64 + x) & 0x0FFF]++;
                }
            }
        }
    }
}
void textures_generate(void)
{
    generate_circle_noise(0x11, 5, 800);
    generate_circle_noise(0x10, 15, 112);
    uint8_t *t12 = g_textures[0x12], *t13 = g_textures[0x13], *t;
    for (int i = 0; i < 4096; i++)
    {
        t12[i] = 20;
        t13[i] = 3;
    }
    for (int i = 0; i < 4032; i++)
        t12[i + 64] = (uint8_t)((rnd_next(4) + t12[i] + t12[i + 1] - 1) >> 1);
    for (int i = 0; i < 4096; i++)
        t13[rnd_next(4096)]++;
    for (int i = 0; i < 30; i++)
    {
        tex01_data[i][0] = rnd_next(256);
        tex01_data[i][1] = rnd_next(0xEC0 + 128) + 64;
    }
    t = g_textures[0];
    for (int i = 0; i < 4096; i++)
    {
        int x = i & 63, y = i >> 6;
        int xx = (x & 8) ? (x ^ 255) & 15 : (x & 15);
        int yy = (y & 8) ? (y ^ 255) & 15 : (y & 15);
        *t++ = (uint8_t)(0xE0 + (xx < yy ? xx : yy));
    }
    const uint8_t *bg_p = TEXTURE_BACKGROUND_TABLE;
    for (int i = 2; i <= 15; i++)
    {
        uint8_t *dst = g_textures[i];
        uint8_t *src = g_textures[(*bg_p++) >> 1];
        int off = *bg_p++;
        for (int j = 0; j < 4096; j++)
            *dst++ = *src++ + (uint8_t)off;
    }
    const uint8_t *pt = TEXTURE_PATTERN_TABLE;
    for (int i = 0; i < 26; i++)
    {
        uint8_t head = *pt++, x0 = *pt++, y0 = *pt++, x1 = *pt++, y1 = *pt++, off = *pt++;
        int mode = head >> 4;                   /* 模式：0=清空, 1=纯色, 2=叠加, 3=棋盘 */
        uint8_t *dst = g_textures[head & 0x0F]; /* 目标纹理索引 */
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
            {
                int idx = (y << 6) | x;
                int a = (mode == 1) ? 0 : (mode == 2 ? dst[idx] : (((x + y) & 4) ? 0x98 : 0));
                dst[idx] = a + off; /* 写入基础值 + 颜色偏移 */
            }
    }
    t = g_textures[0x0B];
    for (int i = 0; i < 20; i++)
    {
        int count = (20 - i) << 3;
        while (count--)
        {
            int p = 0xFFF - (i << 6) - rnd_next(64);
            t[p & 0x0FFF] = (uint8_t)(0x5E - i);
        }
    }
}
static const uint8_t INDEX_TABLE[] = {
    1,5,6,2, 3,7,4,0, 2,6,7,3,
    0,4,5,1, 7,6,5,4, 0,1,2,3,
};
static const int8_t VERTEX_TABLE[] = {
    0xE0,0xE0,0xE0, 0x20,0xE0,0xE0, 0x20,0x20,0xE0, 0xE0,0x20,0xE0,
    0xE0,0xE0,0x20, 0x20,0xE0,0x20, 0x20,0x20,0x20, 0xE0,0x20,0x20,
    0x40,0x00,0x00, 0xC0,0x00,0x00, 0x00,0x40,0x00, 0x00,0xC0,0x00,
    0x00,0x00,0x40, 0x00,0x00,0xC0, 0x00,0x00,0x00,
};
static const uint8_t MODEL_TABLE[] = {
    0x71,0x70,0x00,0x06, 0x72,0x00,0x00,0x06, 0x71,0x00,0x70,0x06,
    0x73,0x07,0x70,0x06, 0x73,0x07,0x00,0x06, 0x70,0x07,0x07,0x06,
    0x74,0x00,0x07,0x06, 0x70,0x00,0x07,0x40, 0x72,0x70,0x07,0x50,
    0x72,0x70,0x00,0x50, 0x71,0x70,0x70,0x50, 0x73,0x00,0x70,0x40,
    0x71,0x00,0x00,0x40, 0x72,0x00,0x00,0x50, 0x71,0x00,0x70,0x50,
    0x73,0x07,0x70,0x46, 0x73,0x00,0x00,0x46, 0x70,0x07,0x07,0x46,
    0x72,0x00,0x07,0x50, 0x71,0x00,0x00,0x00, 0x71,0x00,0x00,0x00,
    0x71,0x00,0xBB,0x56, 0x70,0x0F,0xBB,0x46,
    0xFF,
    0x33,0x00,0x00,0x00, 0xF0,0x00,0x07,0x06,0xE4,
    0xB0,0x00,0xBB,0x46, 0xE4,0x90,0x00,0xBB,0x56,
    0xE4,0x90,0x00,0xBB,0x56, 0xE4,0x30,0x00,0xBB,0x56,
    0xF0,0x00,0xBB,0x56,0x11, 0xF0,0x00,0xBB,0x46,0x11,
    0xB0,0x00,0xBB,0x56,0x11, 0x90,0x00,0xBB,0x56,0x11,
    0xB0,0x00,0xBB,0x56,0x11, 0xF0,0x00,0xBB,0x56,0x11,
    0xF0,0x00,0xBB,0x46,0x11, 0xB0,0x00,0xBB,0x56,0x11,
    0x30,0x00,0xBB,0x56, 0x10,0x00,0xBB,0x56,
    0x34,0x00,0x00,0x00, 0x32,0x05,0x00,0x80,
    0x35,0x05,0x80,0x80, 0x35,0x05,0x80,0x00,
    0x33,0x05,0x00,0x08, 0x33,0x05,0x00,0x08,
    0x74,0x05,0x08,0x08, 0x74,0x05,0x00,0x00,
    0x73,0x05,0x08,0x80, 0x75,0x00,0x00,0x00,
    0x75,0x05,0x08,0x88, 0x70,0x00,0x00,0x00,
    0x74,0x64,0x08,0x08, 0x74,0x60,0x08,0x00,
    0x72,0x64,0x08,0x80, 0x05,0x60,0x00,0x80,
    0x05,0x60,0x00,0x00, 0x02,0x60,0x00,0x08,
    0x34,0x60,0x00,0x08, 0x34,0x60,0x00,0x00,
    0x32,0x60,0x00,0x80, 0x35,0x60,0x00,0x80,
    0x35,0x60,0x00,0x00, 0x32,0x60,0x00,0x08,
    0x34,0x35,0x00,0x08, 0x14,0x65,0x00,0x00,
    0x32,0x35,0x00,0x80, 0x35,0x35,0x00,0x80,
    0x15,0x65,0x00,0x00, 0x32,0x35,0x00,0x08,
    0x34,0x35,0x80,0x08, 0x14,0x65,0x00,0x00,
    0x32,0x35,0x80,0x80, 0x75,0x00,0x00,0x00,
    0x52,0x65,0xE0,0x77, 0x32,0x64,0x0E,0x77,
    0x12,0x65,0x00,0x77, 0x00,0x65,0x00,0x77,
    0xFF,
    0x32,0x00,0x00,0x00, 0xF0,0x00,0x00,0x00,0x14,
    0xB0,0x00,0x00,0x00, 0x14,0x30,0x00,0x00,0x00,
    0xF0,0x00,0x00,0x00, 0xF4,0xF0,0x00,0xDD,0x56,
    0xF4,0x30,0x00,0xDD,0x46, 0x30,0x00,0xDD,0x56,
    0x30,0x00,0xDD,0x56, 0x32,0x00,0x00,0x50,
    0x30,0x0A,0xA0,0x50, 0x33,0x00,0xA0,0x50,
    0x30,0x00,0x00,0x50, 0x32,0x70,0x00,0x50,
    0x30,0x00,0xA0,0x50, 0x33,0xA0,0xA7,0x50,
    0x33,0x00,0x00,0x00, 0x31,0xA0,0x7A,0x50,
    0x31,0x00,0x0A,0x50, 0x31,0x00,0x0A,0x50,
    0x32,0x0A,0x0A,0x50, 0x35,0x00,0x00,0x00,
    0x72,0x0A,0x00,0x00, 0x70,0x0A,0xA0,0x00,
    0x73,0x00,0xA0,0x00, 0x70,0x00,0x00,0x00,
    0x72,0x00,0x00,0x00, 0x70,0x00,0xA0,0x00,
    0x73,0xA0,0xA0,0x00, 0x73,0xA0,0x00,0x99,
    0x71,0xA0,0x0A,0x00, 0x71,0x00,0x0A,0x00,
    0x71,0x00,0x0A,0x00, 0x75,0x0A,0x0A,0x00,
    0x70,0x0C,0x0C,0x03, 0x70,0x00,0x0C,0x03,
    0x70,0x00,0x0C,0x03, 0x72,0xC0,0x7C,0x03,
    0x72,0x00,0x00,0x00, 0x71,0xC0,0xC7,0x03,
    0x73,0x00,0xC0,0x03, 0x71,0x70,0x00,0x03,
    0x72,0x00,0x00,0x03, 0x71,0x00,0xC0,0x03,
    0x73,0x0C,0xC0,0x03, 0x71,0x00,0x00,0x03,
    0x31,0x0C,0xCC,0xA3,
    0xFF,
    0x32,0x00,0x00,0x00, 0xF0,0x00,0x70,0x06,0x14,
    0xB0,0x00,0xDD,0x46, 0x14,0x30,0x00,0xDD,0x56,
    0xB2,0x00,0x0D,0x56, 0xC4,0x32,0xDD,0x00,0x56,
    0x32,0xDD,0x00,0x56, 0x32,0xDD,0x00,0x56,
    0x70,0x00,0xF0,0x56, 0x70,0x00,0xBB,0x56,
    0x32,0xB0,0x0B,0x46, 0x72,0xBB,0x00,0x56,
    0x71,0xB0,0xB0,0x56, 0x71,0x02,0x11,0x56,
    0x71,0x22,0x11,0x56, 0x31,0x20,0x11,0x56,
    0x73,0x0B,0xB0,0x56, 0x73,0xBB,0x00,0x56,
    0x30,0x0B,0x0B,0x46, 0x30,0x00,0xBB,0x56,
    0xFF,
};

/* 从 MODEL_TABLE 生成 3D 模型（顶点和四边形列表）。
   这是整个场景几何的核心解析器，采用类似"海龟建模"的方式：

   模型由 4 个 chunk 组成，每个 chunk 是一个自描述的数据流。
   每个 chunk 内部维护：
   - fp[15][3]: 15 个浮点顶点位置
   - ip[15][4]: 15 个整型顶点（x, y, z, light）
   - fp[14]: 累积位移向量（每步挤出方向）

   解析循环：
   1. 读取命令字节 B
   2. B == 0xFF → chunk 结束
   3. 高4位 = 光照级别
   4. 6个面依次处理：根据 INDEX_TABLE 找到对应的4个顶点
      每个面生成一个 model_quad_t，包含4个顶点索引和纹理索引
   5. 根据低4位的面索引，更新挤出方向（fp[14] += fp[8+face]）
   6. 如果 B & 0x80 设置了旋转标志，对所有14个顶点做旋转变换
   7. 沿挤出方向移动顶点（ip[s] = fp[s] + fp[14]）

   顶点去重：通过比较 (x,y,z) 三元组，相同坐标复用同一个顶点索引。 */
void model_generate(void)
{
    g_vtx_count = g_quad_count = 0;
    uint8_t *m = MODEL_TABLE;
    for (int chunk = 0; chunk < 4; chunk++)
    {
        float fp[15][3];
        int16_t ip[15][4];
        const int8_t *vt = VERTEX_TABLE;
        for (int i = 0; i < 15; i++)
        {
            for (int k = 0; k < 3; k++)
            {
                float v = (float)*vt++;
                fp[i][k] = v;
                ip[i][k] = (int16_t)v;
            }
            ip[i][3] = 0x7F00; /* 初始光照值 */
        }
        while (1)
        {
            uint8_t b = *m++;
            if (b == 0xFF)
                break;                                /* chunk 结束标记 */
            int light = ((b << 8) & 0x7000) | 0x0F00; /* 从命令字节提取光照值 */
            /* 处理 6 个面（立方体的6个面） */
            for (int f = 0; f < 6; f++)
            {
                int tex = (f & 1) ? (*m++ & 0x0F) : (*m >> 4);
                if (!tex--)
                    continue;
                model_quad_t *q = &g_quads[g_quad_count++];
                q->idx[4] = (int16_t)tex;
                for (int v = 0; v < 4; v++)
                {
                    int16_t *src = ip[INDEX_TABLE[f * 4 + v]];
                    /* 顶点去重：遍历已有顶点，匹配 (x,y,z) */
                    int k = 0;
                    for (; k < g_vtx_count; k++)
                    {
                        if (*(int *)src == *(int *)g_vtxs[k].v && src[2] == g_vtxs[k].v[2])
                            break;
                    }
                    if (k == g_vtx_count)
                    { /* 新顶点 */
                        *(int *)g_vtxs[k].v = *(int *)src;
                        *(int *)&g_vtxs[k].v[2] = *(int *)&src[2];
                        g_vtx_count++;
                    }
                    q->idx[v] = (int16_t)k;
                }
            }
            int face = b & 0x0F;
            for (int k = 0; k < 3; k++)
                fp[14][k] += fp[8 + face][k];
            /* 如果设置了旋转标志，对所有顶点进行旋转变换 */
            if (b & 0x80)
            {
                uint8_t r = *m++;
                int16_t angs[3] = {0, 0, 0};
                int ang = (int16_t)(r << 8) & 0xF000;
                if (r & 4)
                    angs[0] = ang; /* X 轴旋转 */
                if (r & 2)
                    angs[1] = ang; /* Y 轴旋转 */
                if (r & 1)
                    angs[2] = ang; /* Z 轴旋转 */
                for (int i = 0; i < 14; i++)
                    rotate_vec3(fp[i], angs);
            }
            /* 更新顶点位置：沿挤出方向移动 */
            for (int i = 0; i < 4; i++)
            {
                int s = INDEX_TABLE[face * 4 + i];
                int d = INDEX_TABLE[(face * 4 + i) ^ 7];
                *(int *)ip[d] = *(int *)ip[s];
                *(int *)&ip[d][2] = *(int *)&ip[s][2];
                for (int k = 0; k < 3; k++)
                    ip[s][k] = (int16_t)(fp[s][k] + fp[14][k]);
                ip[s][3] = (int16_t)light; /* 写入光照值 */
            }
        }
    }
}
typedef struct {
    float   pos[3];
    float   m0[9], m1[9];
    int16_t dir_x;
    int16_t spd[4];
    int     ptr, ctr, dir;
} camera_t;
static const uint8_t CAMERA_TABLE[] = {
    0xA1,0xF8,0xF8,0x98,0xA1,0xA6,0xF0,0xA7, 0xB3,0x37,0x10,0x36,0x50,0x84,0x85,0x85,
    0x84,0xF0,0xF0,0xB2,0xA7,0xA6,0xA6,0xA7, 0xF3,0x26,0x70,0x27,0x80,0x44,0x45,0x77,
    0x76,0xA6,0xB0,0x61,0x61,0x52,0xA7,0x77, 0x30,0x76,0x80,0xA2,0x01,0x41,0x41,0x01,
    0xF5,0x30,0xF4,0xB3,0xA4,0xF8,0xA5,0xD0, 0x47,0x46,0xF0,0xA6,0x80,0xA7,0x70,0xA7,
    0x10,0xA6,0x8E,0xF0,0x50,0x8F,0xB2,0xF8, 0xA3,0x80,0x8E,0xF0,0x50,0x8F,0xB7,0x10,
    0xB6,0xF0,0x80,0xA6,0xA7,0xF0,0x87,0xF0, 0x70,0x86,0x36,0xF0,0xF0,0x90,0x37,0x83,
    0x2C,0xF0,0xF0,0xF0,0x30,0x2D,0x82,0x30, 0xB5,0xB4,0xF2,0xA6,0x30,0xA7,0x83,0x83,
    0xA6,0xF0,0x58,0xA7,0x77,0x76,0xF0,0xF0, 0xF0,0xF0,0xF8,0xF8,0x00,
};
/* 相机路径更新，返回 0 表示路径结束。
   每帧根据当前速度和方向更新相机状态，实现平滑的飞行轨迹。 */
int cam_tick(camera_t *c)
{
    // 1. 路径逻辑保持不变
    if (--c->ctr < 0)
    {
        uint8_t a = CAMERA_TABLE[c->ptr++];
        if (!a)
            return 0;
        c->ctr = (a & 0xF8) << 1;
        if ((c->dir = a & 7) == 1)
            c->dir_x = -c->dir_x;
    }

    // 2. 加速度逻辑
    if (c->dir == 1)
        c->spd[3] += c->dir_x;
    else if (c->dir >= 2)
    {
        c->spd[(c->dir - 2) >> 1] += (c->dir & 1) ? -1 : 1;
    }

    // 3. 构造原始一致的 angles 数组
    // 原始：angs[0]=spd[3], angs[1]=spd[2], angs[2]=spd[1] (均右移2位)
    int16_t angs[3] = {
        (int16_t)(c->spd[3] >> 2),
        (int16_t)(c->spd[2] >> 2),
        (int16_t)(c->spd[1] >> 2)};

    for (int i = 0; i < 3; i++)
    {
        // 初始基向量
        float r[3] = {0};
        r[i] = 1.0f;

        // 调用宏（内部逻辑已修正为原始顺序）
        rotate_vec3(r, angs);

        // 更新矩阵
        for (int j = 0; j < 3; j++)
        {
            float v = r[0] * c->m0[j] + r[1] * c->m0[3 + j] + r[2] * c->m0[6 + j];
            c->m0[i * 3 + j] = c->m1[i * 3 + j] = v;
        }
    }

    // 4. 额外的 Roll 效果 (m1 矩阵)
    // 原始代码：ra = c->spd[2] << 4
    float ra = (float)(c->spd[2] << 4) * int16_to_rad;
    for (int i = 0; i < 3; i++)
    {
        rotate_vec2(&c->m1[i], &c->m1[i + 3], ra);
    }

    // 5. 移动
    float s = (float)c->spd[0] * (1.0f / 1792.0f);
    for (int i = 0; i < 3; i++)
        c->pos[i] += c->m0[6 + i] * s;

    return 1;
}

static const uint8_t MIDI_TABLE[] = {
    // Channel 2: 0x50, 0x0DE3-0x0E51
    0x02, 0x50,
        0xFB, // Repeat x5
            0xFE, // Repeat x2
                0x32, 0x04, 0x45, 0x02, 0x3E, 0x02, 0x45, 0x02,
                0x51, 0x02, 0x3E, 0x02, 0x45, 0x02, 0x4F, 0x02,
                0x3E, 0x02, 0x45, 0x02, 0x51, 0x02, 0x3E, 0x02,
                0x45, 0x02, 0x4F, 0x02, 0x51, 0x02,
            0x00,
            0xFE, // Repeat x2
                0x32, 0x04, 0x46, 0x02, 0x3E, 0x02, 0x46, 0x02,
                0x51, 0x02, 0x3E, 0x02, 0x46, 0x02, 0x4F, 0x02,
                0x3E, 0x02, 0x46, 0x02, 0x52, 0x02, 0x3E, 0x02,
                0x46, 0x02, 0x4F, 0x02, 0x52, 0x02,
            0x00,
        0x00,
        0x37, 0x20, 0x38, 0x20, 0x33, 0x20, 0x35, 0x18,
        0x37, 0x08, 0x39, 0x20, 0x32, 0x20, 0x3C, 0x30,
        0x3A, 0x08, 0x39, 0x08, 0x37, 0x40,
        0xFE, // Repeat x2
            0x43, 0x10, 0x3E, 0x10, 0x45, 0x10, 0x46, 0x10,
        0x00,
        0xFE, // Repeat x2
            0x46, 0x10, 0x45, 0x10, 0x3E, 0x10, 0x41, 0x10,
        0x00,
    0x00,
    // Channel 4: 0x32, 0x0E52-0x0EA0
    0x04, 0x32,
        0x01, 0x80, 0x01, 0x80, 0x41, 0x1C, 0x40, 0x02,
        0x41, 0x02, 0x40, 0x10, 0x3E, 0x08, 0x3C, 0x08,
        0x43, 0x1C, 0x41, 0x04, 0x3C, 0x10, 0x41, 0x08,
        0x40, 0x08, 0x3E, 0x20, 0x32, 0x10, 0x40, 0x08,
        0x41, 0x08, 0x43, 0x20, 0x3C, 0x10, 0x3E, 0x08,
        0x40, 0x08, 0x3E, 0x80, 0x43, 0x80, 0x43, 0x10,
        0x40, 0x10, 0x42, 0x10, 0x40, 0x08, 0x42, 0x08,
        0x48, 0x30, 0x46, 0x10, 0x43, 0x20, 0x37, 0x20,
        0xFE, // Repeat x2
            0x1F, 0x20, 0x37, 0x20,
        0x00,
        0xFE, // Repeat x2
            0x1A, 0x20, 0x32, 0x20,
        0x00,
    0x00,
    // Channel 5: 0x59, 0x0EA1-0x0EE1
    0x05, 0x59,
        0xFB, // Repeat x5
            0x01, 0x80,
        0x00,
        0x43, 0x08, 0x4A, 0x18, 0x44, 0x08, 0x4B, 0x18,
        0x3F, 0x08, 0x46, 0x18, 0x3C, 0x08, 0x45, 0x10,
        0x43, 0x08, 0x3E, 0x08, 0x45, 0x10, 0x43, 0x08,
        0x3E, 0x08, 0x42, 0x18, 0x3F, 0x08, 0x43, 0x18,
        0x3C, 0x08, 0x43, 0x10, 0x45, 0x08, 0x37, 0x08,
        0x43, 0x18, 0x2B, 0x08, 0x32, 0x18,
        0xFE, // Repeat x2
            0x1F, 0x01, 0x26, 0x3F,
        0x00,
        0xFE, // Repeat x2
            0x1A, 0x01, 0x21, 0x3F,
        0x00,
    0x00,
    // Channel 10: 0x00, 0x0EE2-0x0F26
    0x0A, 0x00,
        0xF0, // Repeat x16
            0x2A, 0x02, 0x2A, 0x02, 0x2A, 0x02, 0x2A, 0x01,
            0x2A, 0x01,
        0x00,
        0xF0, // Repeat x16
            0x24, 0x02, 0x2A, 0x02, 0x2A, 0x02, 0x2A, 0x01,
            0x2A, 0x01,
        0x00,
        0xE2, // Repeat x30
            0x24, 0x02, 0x2A, 0x02, 0x2E, 0x02, 0x24, 0x01,
            0x2A, 0x01, 0x26, 0x02, 0x2A, 0x02, 0x2E, 0x02,
            0x26, 0x01, 0x2A, 0x01, 0x24, 0x02, 0x26, 0x02,
            0x24, 0x02, 0x2A, 0x01, 0x2A, 0x01, 0x26, 0x02,
            0x2A, 0x02, 0x2E, 0x02, 0x26, 0x01, 0x2A, 0x01,
        0x00,
    0x00
};
#define MIDI_DATA_MAX 2048
typedef struct
{
    int channel, instrument, data_len;
    uint8_t data[MIDI_DATA_MAX];
} midi_channel_t;
typedef struct
{
    midi_channel_t ch[MIDI_MAX_CH];
    int idx[MIDI_MAX_CH][2];
    HMIDIOUT hmo;
} midi_state_t;
static midi_state_t ms;
static uint8_t *g_ptr;
static uint8_t *g_dst;
static void decompress_loop()
{
    while (*g_ptr != 0x00)
    {
        uint8_t val = *g_ptr++;
        if (val >= 0x80)
        {
            int count = 0x100 - val;
            uint8_t *restart_point = g_ptr;
            while (count--)
            {
                g_ptr = restart_point;
                decompress_loop();
            }
        }
        else
        {
            *g_dst++ = val;
            *g_dst++ = *g_ptr++;
        }
    }
    g_ptr++;
}
static HDC g_hdc_mem;
static HBITMAP g_h_dib;
static uint8_t *g_dib_bits;
static void sw_convert(const uint8_t *src, int y0, int y1)
{
    uint8_t *dib = g_dib_bits + y0 * 960;
    const uint8_t *e = src + (y1 + 1) * 320;
    uint8_t *src_start = src + y0 * 320;
    while (src_start < e)
    {
        int c = *src_start * 3;
        dib[0] = g_palette[c + 2];
        dib[1] = g_palette[c + 1];
        dib[2] = g_palette[c];
        src_start++;
        dib += 3;
    }
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
static void sw_draw_quad(int qi)
{
    /* 步骤1-4：收集顶点数据 */
    for (int i = 0; i < 4; i++)
    {
        int vi = g_quads[qi].idx[i];
        v[i][0] = g_tvtxs[vi * 3 + 0]; /* X */
        v[i][1] = g_tvtxs[vi * 3 + 1]; /* Y */
        v[i][2] = g_tvtxs[vi * 3 + 2]; /* Z */
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
LPCSTR name = "OMNISCENT";
LPCSTR copyright = "(C) 1997 SANCTION";
void generate_text(void)
{
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hbm = CreateCompatibleBitmap(GetDC(0), 320, 200);
    SelectObject(hdc, hbm);
    HFONT hFont = CreateFontA(8, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              OEM_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
                              DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Terminal");
    SelectObject(hdc, hFont);
    SetBkMode(hdc, OPAQUE);
    SetTextColor(hdc, RGB(0x1C, 0x1C, 0x1C));
    TextOutA(hdc, 48, 8, name, 9);
    TextOutA(hdc, 8, 96, copyright, 17);
    for (int i = 0; i < 0x7D00; i++)
    {
        int x = i % 320;
        int y = (i - x) / 320 + 5;
        g_bg[i * 2] = g_bg[i * 2 + 1] = GetPixel(hdc, x, y);
    }
}
void __cdecl main(void)
{
    HWND hwnd = CreateWindowExA(0, (LPCSTR)0xC018, 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0);
    HDC hdc = GetDC(hwnd);
    palette_generate();
    textures_generate();
    model_generate();
    generate_text();
    g_ptr = (uint8_t *)MIDI_TABLE;
    ms.hmo = 0;
    for (int i = 0; i < MIDI_MAX_CH && *g_ptr != 0; i++)
    {
        ms.ch[i].channel = *g_ptr++ - 1;
        ms.ch[i].instrument = *g_ptr++;
        g_dst = ms.ch[i].data;
        decompress_loop();
        ms.ch[i].data_len = g_dst - ms.ch[i].data;
    }
    g_hdc_mem = CreateCompatibleDC(0);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = FB_W;
    bmi.bmiHeader.biHeight = -FB_H; /* 负高度 = 自上而下的 DIB */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    g_h_dib = CreateDIBSection(g_hdc_mem, &bmi, DIB_RGB_COLORS, (void **)&g_dib_bits, NULL, 0);
    SelectObject(g_hdc_mem, g_h_dib);
    sw_convert(g_bg, 0, 199);
    midiOutOpen(&ms.hmo, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    for (int i = 0; i < MIDI_MAX_CH; i++)
    {
        ms.idx[i][0] = -2;
        ms.idx[i][1] = 0;
        if (ms.hmo)
            midiOutShortMsg(ms.hmo, (DWORD)(0xC0 | ms.ch[i].channel) | (ms.ch[i].instrument << 8));
    }
    int tick_accum = 0, r = 0, stars = 0;
    camera_t cam = {0};
    cam.pos[0] = -(160.0f * 2.0f + 10.0f);
    cam.pos[2] = 64.0f;
    cam.m0[1] = -1;
    cam.m0[5] = -1;
    cam.m0[6] = 1;
    memcpy(cam.m1, cam.m0, 36);
    cam.dir_x = 1;
    DWORD g_last_tick = GetTickCount();
    while (!GetAsyncKeyState(VK_ESCAPE))
    {
        PeekMessage(0, 0, 0, 0, PM_REMOVE);
        DWORD now = GetTickCount();
        int current_tick = (int)((now - g_last_tick) * 35 / 100);
        while (tick_accum < current_tick)
        {
            for (int i = 0; i < MIDI_MAX_CH; i++)
            {
                if (ms.idx[i][0] >= ms.ch[i].data_len)
                    continue;
                if (ms.idx[i][1] == 0)
                {
                    ms.idx[i][0] += 2;
                    int d = ms.idx[i][0];
                    if (d > 0 && ms.hmo && midiOutShortMsg(ms.hmo, (DWORD)(0x80 | ms.ch[i].channel) | (ms.ch[i].data[d - 2] << 8) | (0x7F << 16)))
                    {
                        ms.hmo = 0;
                        break;
                    }
                    if (d >= ms.ch[i].data_len)
                        continue;
                    ms.idx[i][1] = ms.ch[i].data[d + 1] * 29;
                    if (ms.hmo && midiOutShortMsg(ms.hmo, (DWORD)(0x90 | ms.ch[i].channel) | (ms.ch[i].data[d] << 8) | (0x7F << 16)))
                    {
                        ms.hmo = 0;
                        break;
                    }
                }
                ms.idx[i][1]--;
            }
            r = (r + 1) & 7;
            if (!r)
            {
                {
                    uint8_t t0 = g_palette[0xC0 * 3], t1 = g_palette[0xC0 * 3 + 1];
                    for (int i = 0; i < 0x5D; i++)
                        g_palette[0xC0 * 3 + i] = g_palette[0xC1 * 3 + i];
                    g_palette[0xDF * 3] = t0;
                    g_palette[0xDF * 3 + 1] = t1;
                }
                {
                    memset(g_textures[0x01], 0, TEX_SIZE);
                    for (int i = 0; i < 0x1E; i++)
                    {
                        tex01_data[i][0] = (tex01_data[i][0] - 1) & 0xFF;
                        int a = tex01_data[i][0] & 0x3F;
                        if (a < 0x1F)
                            a = (~a) & 0x1F;
                        a >>= 1;
                        draw_star(g_textures[0x01], tex01_data[i][1], 0x003B, 0xE000, a);
                    }
                    g_door_counter++;
                }
            }
            if (!cam_tick(&cam))
            {
                goto done;
            }
            tick_accum++;
        }
        if (g_door_counter >= 0 && !stars)
        {
            stars = 1;
            for (int i = 0; i < 100; i++)
            {
                int p = rnd_next(0xC1C0) + 0x1900;
                draw_star(g_bg, p, 0x013B, 0x0002, (p >> 8) & 0x07);
            }
        }
        int offset = g_door_counter;
        if (offset < 0)
            offset = 0;
        if (offset > 0x18)
            offset = 0x18;
        offset <<= 6;
        for (int i = 0; i < 0x800; i++)
        {
            g_textures[0x0D][i] = g_textures[0x0F][offset + i];
            g_textures[0x0D][0x800 + i] = g_textures[0x0F][0x800 - offset + i];
        }
        for (int i = 0; i < offset * 2; i++)
            g_textures[0x0D][0x800 - offset + i] = 0;
        memcpy(g_fb + 20 * 320, g_bg + 20 * 320, 320 * 160);
        const float *cp = cam.pos, *cm = cam.m1;
        for (int i = 0; i < g_vtx_count; i++)
            for (int j = 0; j < 3; j++)
                g_tvtxs[i * 3 + j] = (g_vtxs[i].v[0] - cp[0]) * cm[j * 3] + (g_vtxs[i].v[1] - cp[1]) * cm[j * 3 + 1] + (g_vtxs[i].v[2] - cp[2]) * cm[j * 3 + 2];
        for (int i = 0; i < g_quad_count; i++)
        {
            int z = 0;
            for (int j = 0; j < 4; j++)
            {
                int vi = g_quads[i].idx[j];
                z -= (int)g_tvtxs[vi * 3 + 2]; /* 负 Z 值之和 */
            }
            g_sort_list[i * 2] = (int16_t)z;
            g_sort_list[i * 2 + 1] = (int16_t)i;
        }
        qsort_pairs(g_sort_list, 0, g_quad_count - 1);
        for (int i = 0; i < g_quad_count; i++)
            sw_draw_quad(g_sort_list[i * 2 + 1]);
        sw_convert(g_fb, 20, 20 + 160);
        StretchBlt(hdc, 0, 0, GetSystemMetrics(0), GetSystemMetrics(1), g_hdc_mem, 0, 0, 320, 200, SRCCOPY);
        Sleep(1);
    }
done:
    ExitProcess(0);
}