#ifndef OMNISCENT_H
#define OMNISCENT_H

#include <windows.h>

typedef struct {
    int stopping;
    int use_gl;
    int renderer_counter;
    int bg_star_drawn;
} omniscent_t;

void omniscent_init(omniscent_t *o);
int  omniscent_on_timer(omniscent_t *o);
void omniscent_render(omniscent_t *o);

#endif
