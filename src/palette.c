#include "palette.h"

uint8_t g_palette[PAL_SIZE];

static const uint8_t PALETTE_TABLE[] = {
    0x10,
    0x1F, 0x3F, 0x3F, 0x3F,
    0x01, 0x00, 0x00, 0x00,
    0x1F, 0x28, 0x20, 0x3F,
    0x01, 0x00, 0x00, 0x00,
    0x1F, 0x3F, 0x00, 0x00,
    0x01, 0x06, 0x01, 0x00,
    0x1F, 0x3F, 0x29, 0x14,
    0x01, 0x00, 0x00, 0x00,
    0x1F, 0x3F, 0x3F, 0x08,
    0x01, 0x00, 0x00, 0x00,
    0x1F, 0x38, 0x38, 0x3F,
    0x01, 0x3F, 0x00, 0x00,
    0x10, 0x3F, 0x3F, 0x00,
    0x10, 0x3F, 0x00, 0x00,
    0x01, 0x16, 0x05, 0x00,
    0x07, 0x3F, 0x38, 0x11,
};

void palette_generate(void) {
    int a = 0x0000, b = 0x0001;
    int p = 0, q = 0;

    for (int i = 0; i < 3; i++) {
        p = 0;
        q = 0;
        a = PALETTE_TABLE[0];
        b &= 0x00FF;
        int entry_count = PALETTE_TABLE[p++];
        for (int j = 0; j < entry_count; j++) {
            a = (a & 0x00FF) | (PALETTE_TABLE[p + i + 1] << 8);
            a = ((a - b) / PALETTE_TABLE[p]);
            for (int k = 0; k < PALETTE_TABLE[p]; k++, q++) {
                b += a;
                g_palette[q * 3 + i] = (uint8_t)(b >> 8);
            }
            p += 4;
        }
    }

    for (int i = 0; i < PAL_SIZE; i++)
        g_palette[i] = (uint8_t)((g_palette[i] << 2) | (g_palette[i] >> 4));
}

void palette_tick(void) {
    uint8_t t0 = g_palette[0xC0 * 3];
    uint8_t t1 = g_palette[0xC0 * 3 + 1];
    for (int i = 0; i < 0x5D; i++) {
        g_palette[0xC0 * 3 + i] = g_palette[0xC1 * 3 + i];
    }
    g_palette[0xDF * 3] = t0;
    g_palette[0xDF * 3 + 1] = t1;
}
