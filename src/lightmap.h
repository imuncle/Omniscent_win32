#ifndef OMNISCENT_LIGHTMAP_H
#define OMNISCENT_LIGHTMAP_H

#include <stdint.h>

#define LM_SIZE (128 * 256)

extern uint8_t g_lightmap[LM_SIZE];

void lightmap_generate(void);

#endif
