#ifndef OMNISCENT_PALETTE_H
#define OMNISCENT_PALETTE_H

#include <stdint.h>

#define PAL_SIZE (3 * 256)

extern uint8_t g_palette[PAL_SIZE];

void palette_generate(void);
void palette_tick(void);

#endif
