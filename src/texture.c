#include "texture.h"
#include "random.h"
#include "util.h"

uint8_t g_textures[TEX_COUNT][TEX_SIZE];
int16_t g_door_counter = -4250;

static int tex01_data[0x1E][2];

static const uint8_t TEXTURE_PATTERN_TABLE[] = {
    0x13, 0x0F, 0x0F, 0x31, 0x31, 0x0A, 0x13, 0x10, 0x10, 0x30, 0x30, 0x11,
    0x13, 0x11, 0x11, 0x2F, 0x2F, 0x18, 0x13, 0x12, 0x12, 0x2E, 0x2E, 0xBE,
    0x13, 0x15, 0x15, 0x2B, 0x2B, 0x1E, 0x25, 0x00, 0x00, 0x3E, 0x1E, 0x03,
    0x25, 0x03, 0x03, 0x3E, 0x1E, 0x07, 0x25, 0x03, 0x03, 0x3B, 0x1B, 0xFC,
    0x25, 0x00, 0x20, 0x3E, 0x3E, 0x03, 0x25, 0x03, 0x23, 0x3E, 0x3E, 0x07,
    0x25, 0x03, 0x23, 0x3B, 0x3B, 0xFC, 0x25, 0x00, 0x00, 0x3F, 0x3F, 0xFE,
    0x2C, 0x00, 0x0A, 0x3F, 0x0F, 0xF6, 0x2C, 0x00, 0x0B, 0x3F, 0x10, 0x04,
    0x2C, 0x00, 0x2F, 0x3F, 0x34, 0xF6, 0x2C, 0x00, 0x30, 0x3F, 0x35, 0x04,
    0x2E, 0x13, 0x18, 0x2C, 0x27, 0x05, 0x2E, 0x14, 0x19, 0x2C, 0x27, 0xF4,
    0x2E, 0x14, 0x19, 0x2B, 0x26, 0x07, 0x3E, 0x14, 0x19, 0x2B, 0x26, 0x06,
    0x2F, 0x00, 0x18, 0x3F, 0x1E, 0x06, 0x2F, 0x00, 0x19, 0x3F, 0x1F, 0xFA,
    0x3F, 0x00, 0x19, 0x3F, 0x1E, 0x06, 0x2F, 0x00, 0x20, 0x3F, 0x26, 0x06,
    0x2F, 0x00, 0x21, 0x3F, 0x27, 0xFA, 0x3F, 0x00, 0x21, 0x3F, 0x26, 0x06,
};

static const uint8_t TEXTURE_BACKGROUND_TABLE[] = {
    0x22, 0xC0, 0x26, 0x01, 0x26, 0x01, 0x26, 0x00,
    0x20, 0x00, 0x20, 0x20, 0x20, 0x40, 0x22, 0x60,
    0x24, 0x00, 0x22, 0x60, 0x24, 0x00, 0x24, 0x00,
    0x24, 0x00, 0x24, 0x00,
};

void generate_circle_noise(int id, int r, int n) {
    uint8_t *tex = g_textures[id];
    memset(tex, 0, TEX_SIZE);
    int r2 = r * r;
    while (n--) {
        int p = rnd_next(0x1000); 
        for (int y = -r; y < r; y++) {
            for (int x = -r; x < r; x++) {
                if (x*x + y*y <= r2) {
                    tex[(p + y * 64 + x) & 0x0FFF]++;
                }
            }
        }
    }
}

void textures_generate(void) {
    generate_circle_noise(0x11, 5, 800);
    generate_circle_noise(0x10, 15, 112);
    uint8_t *t12 = g_textures[0x12], *t13 = g_textures[0x13], *t;
    for (int i = 0; i < 4096; i++) {
        t12[i] = 20; t13[i] = 3;
    }
    for (int i = 0; i < 4032; i++) 
        t12[i + 64] = (uint8_t)((rnd_next(4) + t12[i] + t12[i + 1] - 1) >> 1);
    for (int i = 0; i < 4096; i++) 
        t13[rnd_next(4096)]++;
    for (int i = 0; i < 30; i++) {
        tex01_data[i][0] = rnd_next(256);
        tex01_data[i][1] = rnd_next(0xEC0 + 128) + 64;
    }
    t = g_textures[0];
    for (int i = 0; i < 4096; i++) {
        int x = i & 63, y = i >> 6;
        int xx = (x & 8) ? (x ^ 255) & 15 : (x & 15);
        int yy = (y & 8) ? (y ^ 255) & 15 : (y & 15);
        *t++ = (uint8_t)(0xE0 + (xx < yy ? xx : yy));
    }
    const uint8_t *bg_p = TEXTURE_BACKGROUND_TABLE;
    for (int i = 2; i <= 15; i++) {
        uint8_t *dst = g_textures[i];
        uint8_t *src = g_textures[(*bg_p++) >> 1];
        int off = *bg_p++;
        for (int j = 0; j < 4096; j++) *dst++ = *src++ + (uint8_t)off;
    }
    const uint8_t *pt = TEXTURE_PATTERN_TABLE;
    for (int i = 0; i < 26; i++) {
        uint8_t head = *pt++, x0 = *pt++, y0 = *pt++, x1 = *pt++, y1 = *pt++, off = *pt++;
        int mode = head >> 4;                           /* 模式：0=清空, 1=纯色, 2=叠加, 3=棋盘 */
        uint8_t *dst = g_textures[head & 0x0F];        /* 目标纹理索引 */
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                int idx = (y << 6) | x;
                int a = (mode == 1) ? 0 : (mode == 2 ? dst[idx] : (((x + y) & 4) ? 0x98 : 0));
                dst[idx] = a + off;                     /* 写入基础值 + 颜色偏移 */
            }
    }
    t = g_textures[0x0B];
    for (int i = 0; i < 20; i++) {
        int count = (20 - i) << 3;
        while (count--) {
            int p = 0xFFF - (i << 6) - rnd_next(64);
            t[p & 0x0FFF] = (uint8_t)(0x5E - i);
        }
    }
}

void textures_tick(void) {
    memset(g_textures[0x01], 0, TEX_SIZE);
    for (int i = 0; i < 0x1E; i++) {
        tex01_data[i][0] = (tex01_data[i][0] - 1) & 0xFF;
        int a = tex01_data[i][0] & 0x3F;
        if (a < 0x1F) a = (~a) & 0x1F;
        a >>= 1;
        int b = tex01_data[i][1];
        draw_star(g_textures[0x01], b, 0x003B, 0xE000, a);
    }
    g_door_counter++;
}

void textures_update_door(void) {
    int offset = g_door_counter;
    if (offset < 0) offset = 0;
    if (offset > 0x18) offset = 0x18;
    offset <<= 6;
    for (int i = 0; i < 0x800; i++)
        g_textures[0x0D][i] = g_textures[0x0F][offset + i];
    for (int i = 0; i < 0x800; i++)
        g_textures[0x0D][0x800 + i] = g_textures[0x0F][0x800 - offset + i];
    for (int i = 0; i < offset * 2; i++)
        g_textures[0x0D][0x800 - offset + i] = 0x00;
}
