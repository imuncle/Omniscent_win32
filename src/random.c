#include "random.h"

static uint32_t rnd_a = 0x08088405;
static uint32_t rnd_b = 0x000010FB;

int rnd_next(int x) {
    rnd_b = rnd_b * rnd_a + 1;
    return (int)((((rnd_b >> 16) & 0xFFFF) * x) >> 16) & 0xFFFF;
}
