#include "camera.h"
#include "util.h"

static const uint8_t CAMERA_TABLE[] = {
    0xA1, 0xF8, 0xF8, 0x98, 0xA1, 0xA6, 0xF0, 0xA7,
    0xB3, 0x37, 0x10, 0x36, 0x50, 0x84, 0x85, 0x85,
    0x84, 0xF0, 0xF0, 0xB2, 0xA7, 0xA6, 0xA6, 0xA7,
    0xF3, 0x26, 0x70, 0x27, 0x80, 0x44, 0x45, 0x77,
    0x76, 0xA6, 0xB0, 0x61, 0x61, 0x52, 0xA7, 0x77,
    0x30, 0x76, 0x80, 0xA2, 0x01, 0x41, 0x41, 0x01,
    0xF5, 0x30, 0xF4, 0xB3, 0xA4, 0xF8, 0xA5, 0xD0,
    0x47, 0x46, 0xF0, 0xA6, 0x80, 0xA7, 0x70, 0xA7,
    0x10, 0xA6, 0x8E, 0xF0, 0x50, 0x8F, 0xB2, 0xF8,
    0xA3, 0x80, 0x8E, 0xF0, 0x50, 0x8F, 0xB7, 0x10,
    0xB6, 0xF0, 0x80, 0xA6, 0xA7, 0xF0, 0x87, 0xF0,
    0x70, 0x86, 0x36, 0xF0, 0xF0, 0x90, 0x37, 0x83,
    0x2C, 0xF0, 0xF0, 0xF0, 0x30, 0x2D, 0x82, 0x30,
    0xB5, 0xB4, 0xF2, 0xA6, 0x30, 0xA7, 0x83, 0x83,
    0xA6, 0xF0, 0x58, 0xA7, 0x77, 0x76, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF8, 0xF8, 0x00,
};

void cam_init(camera_t *c) {
    c->pos[0] = -330.0f; c->pos[1] = 0.0f; c->pos[2] = 64.0f;
    float init_m[9] = {0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f};
    for (uint8_t i = 0; i < 9; i++) {
        c->m0[i] = init_m[i];
        c->m1[i] = init_m[i];
    }
    c->dir_x = 1;
    c->spd[0] = c->spd[1] = c->spd[2] = c->spd[3] = 0;
    c->ptr = 0;
    c->ctr = 0;
    c->dir = 0;
}

int cam_tick(camera_t *c) {
    // 1. 路径逻辑保持不变
    if (--c->ctr < 0) {
        uint8_t a = CAMERA_TABLE[c->ptr++];
        if (!a) return 0;
        c->ctr = (a & 0xF8) << 1;
        if ((c->dir = a & 7) == 1) c->dir_x = -c->dir_x;
    }

    // 2. 加速度逻辑
    if (c->dir == 1) c->spd[3] += c->dir_x;
    else if (c->dir >= 2) {
        c->spd[(c->dir - 2) >> 1] += (c->dir & 1) ? -1 : 1;
    }

    // 3. 构造原始一致的 angles 数组
    // 原始：angs[0]=spd[3], angs[1]=spd[2], angs[2]=spd[1] (均右移2位)
    int16_t angs[3] = { 
        (int16_t)(c->spd[3] >> 2), 
        (int16_t)(c->spd[2] >> 2), 
        (int16_t)(c->spd[1] >> 2) 
    };

    for (int i = 0; i < 3; i++) {
        // 初始基向量
        float r[3] = {0};
        r[i] = 1.0f;

        // 调用宏（内部逻辑已修正为原始顺序）
        rotate_vec3(r, angs);

        // 更新矩阵
        for (int j = 0; j < 3; j++) {
            float v = r[0] * c->m0[j] + r[1] * c->m0[3+j] + r[2] * c->m0[6+j];
            c->m0[i*3+j] = c->m1[i*3+j] = v;
        }
    }

    // 4. 额外的 Roll 效果 (m1 矩阵)
    // 原始代码：ra = c->spd[2] << 4
    float ra = (float)(c->spd[2] << 4) * int16_to_rad; 
    for (int i = 0; i < 3; i++) {
        rotate_vec2(&c->m1[i], &c->m1[i+3], ra);
    }

    // 5. 移动
    float s = (float)c->spd[0] * (1.0f / 1792.0f);
    for (int i = 0; i < 3; i++) c->pos[i] += c->m0[6+i] * s;

    return 1;
}
