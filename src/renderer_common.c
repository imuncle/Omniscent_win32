#include "renderer_common.h"
#include "util.h"
#include "model.h"
#include <windows.h>

uint8_t g_fb[FB_SIZE];
uint8_t g_bg[FB_SIZE];
int16_t g_sort_list[(367 + 1) * 2];

void generate_text(void) {
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hbm = CreateCompatibleBitmap(GetDC(0), 320, 200);
    SelectObject(hdc, hbm);
    HFONT hFont = CreateFontA(8, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        OEM_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH|FF_MODERN, "Terminal");
    SelectObject(hdc, hFont);
    SetBkMode(hdc, OPAQUE);
    SetTextColor(hdc, RGB(0x1C,0x1C,0x1C));
    SetBkColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, 48, 2, "OMNISCENT", 9);
    TextOutA(hdc, 8, 91, "(C) 1997 SANCTION", 17);
    for (int i = 0; i < 0x7D00; i++)
    {
        int x = i % 320;
        int y = (i - x) / 320;
        g_bg[i * 2] = g_bg[i * 2 + 1] = GetPixel(hdc, x, y);
    }
}

void renderer_sort_quads(void) {
    for (int i = 0; i < g_quad_count; i++) {
        int z = 0;
        for (int j = 0; j < 4; j++) {
            int vi = g_quads[i].idx[j];
            z -= (int)g_tvtxs[vi * 3 + 2];
        }
        g_sort_list[i * 2] = (int16_t)z;
        g_sort_list[i * 2 + 1] = (int16_t)i;
    }
    qsort_pairs(g_sort_list, 0, g_quad_count - 1);
}
