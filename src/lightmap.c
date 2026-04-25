#include "lightmap.h"

uint8_t g_lightmap[LM_SIZE];

void lightmap_generate(void) {
    int p = 0;
    for (int i = 0x00; i < 0x80; i++) {
        for (int j = 0x00; j < 0xC0; j++)
            g_lightmap[p++] = (uint8_t)((j & 0xE0) + (((j & 0x1F) * i * 2) >> 8));
        for (int j = 0xC0; j < 0x100; j++)
            g_lightmap[p++] = (uint8_t)j;
    }
}
