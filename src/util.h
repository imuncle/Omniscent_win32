#ifndef OMNISCENT_UTIL_H
#define OMNISCENT_UTIL_H

#include <stdint.h>

#define my_roundf(x) ((int)((x) + ((x) >= 0 ? 0.5f : -0.5f)))

extern const uint8_t STAR_PATTERN[25];
extern const float int16_to_rad;
void  rotate_vec2(float* v1, float* v2, float angle_rad);
void  rotate_vec3(float v[3], int16_t angles_int16[3]);
void  qsort_pairs(int16_t data[], int l, int r);
void  draw_star(uint8_t buf[], int offset, int stride, int color, int value);

#endif
