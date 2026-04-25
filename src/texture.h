#ifndef OMNISCENT_TEXTURE_H
#define OMNISCENT_TEXTURE_H

#include <stdint.h>

#define TEX_COUNT 20
#define TEX_SIZE  (64 * 64)

extern uint8_t g_textures[TEX_COUNT][TEX_SIZE];
extern int16_t g_door_counter;

void textures_generate(void);
void textures_tick(void);
void textures_update_door(void);
static inline int textures_is_door_opened(void) { return g_door_counter >= 0; }

#endif
