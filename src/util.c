#include "util.h"

/* Compiler-generated runtime helpers */
#if defined(_MSC_VER)
#pragma function(memcpy, memmove, memset)
int _fltused = 0;
__declspec(naked) void _ftol2(void) {
    __asm fistp dword ptr [esp-4]
    __asm mov eax, [esp-4]
    __asm ret
}
#endif
__declspec(naked) void* memset(void* dst, int val, size_t count) {
    __asm { push edi } __asm { mov edi,[esp+8] } __asm { mov eax,[esp+12] } __asm { mov ecx,[esp+16] } __asm { rep stosb } __asm { mov eax,[esp+8] } __asm { pop edi } __asm { ret }
}
void ___chkstk_ms(void) { }
void ___chkstk() { }

const uint8_t STAR_PATTERN[25] = {
    0x00, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x02, 0x05, 0x02, 0x00,
    0x03, 0x05, 0x07, 0x05, 0x03,
    0x00, 0x02, 0x05, 0x02, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00,
};

void rotate_vec2(float* v1, float* v2, float angle_rad) {
    float _s, _o, _t1, _t2;
    __asm { fld angle_rad }
    __asm { fsincos }
    __asm { fstp _o }
    __asm { fstp _s }
    _t1 = *v1; _t2 = *v2;
    *v1 = _o * _t1 - _s * _t2;
    *v2 = _s * _t1 + _o * _t2;
}
const float int16_to_rad = 4.79368996e-5f;
void rotate_vec3(float v[3], int16_t angles_int16[3]) {
    float _a2 = (float)angles_int16[2] * int16_to_rad;
    float _a1 = (float)angles_int16[1] * int16_to_rad;
    float _a0 = (float)angles_int16[0] * int16_to_rad;
    rotate_vec2(&v[1], &v[2], _a2); /* Step 1: y-z 平面 */
    rotate_vec2(&v[2], &v[0], _a1); /* Step 2: z-x 平面 */
    rotate_vec2(&v[0], &v[1], _a0); /* Step 3: x-y 平面 */
}

void qsort_pairs(int16_t data[], int l, int r) {
    if (l >= r) return;
    int ll = l, rr = r;
    int16_t x = data[ll * 2];
    do {
        while (data[ll * 2] > x) ll++;
        while (data[rr * 2] < x) rr--;
        if (ll > rr) break;
        int16_t t0 = data[ll * 2], t1 = data[ll * 2 + 1];
        data[ll * 2] = data[rr * 2]; data[ll * 2 + 1] = data[rr * 2 + 1];
        data[rr * 2] = t0; data[rr * 2 + 1] = t1;
        ll++; rr--;
    } while (ll <= rr);
    qsort_pairs(data, l, rr);
    qsort_pairs(data, ll, r);
}

void draw_star(uint8_t buf[], int offset, int stride, int color, int value) {
    int index = offset;
    int p = 0;
    for (int i = 0; i < 5; i++, index += stride) {
        for (int j = 0; j < 5; j++, p++, index++) {
            uint8_t x = STAR_PATTERN[p];
            if (x > value)
                buf[index] = (uint8_t)(((x - value) + (color >> 8)) << (color & 0xFF));
        }
    }
}
