#ifndef OMNISCENT_RENDERER_COMMON_H
#define OMNISCENT_RENDERER_COMMON_H

#include <stdint.h>

#define FB_W 320
#define FB_H 200
#define FB_SIZE (FB_W * FB_H)

extern uint8_t g_fb[FB_SIZE];
extern uint8_t g_bg[FB_SIZE];
extern int16_t g_sort_list[(367 + 1) * 2];

void generate_text(void);
void renderer_sort_quads(void);

#endif
