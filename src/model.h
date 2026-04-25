#ifndef OMNISCENT_MODEL_H
#define OMNISCENT_MODEL_H

#include <stdint.h>

#define MAX_VTX  362
#define MAX_QUAD 367

typedef struct {
    int16_t v[4];
} model_vertex_t;

typedef struct {
    int16_t idx[5];
} model_quad_t;

extern int g_vtx_count;
extern int g_quad_count;
extern model_vertex_t g_vtxs[MAX_VTX];
extern model_quad_t g_quads[MAX_QUAD];
extern float g_tvtxs[MAX_VTX * 3];

void model_generate(void);
void model_transform(const float cam_pos[3], const float cam_matrix[9]);

#endif
