#ifndef OMNISCENT_CAMERA_H
#define OMNISCENT_CAMERA_H

#include <stdint.h>

typedef struct {
    float    pos[3];
    float    m0[9], m1[9];
    int16_t  dir_x;
    int16_t  spd[4];
    int      ptr;
    int      ctr;
    int      dir;
} camera_t;

void cam_init(camera_t *c);
int  cam_tick(camera_t *c);
static inline const float* cam_get_pos(const camera_t *c) { return c->pos; }
static inline const float* cam_get_matrix(const camera_t *c) { return c->m1; }

#endif
