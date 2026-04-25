#ifndef OMNISCENT_RENDERER_SW_H
#define OMNISCENT_RENDERER_SW_H

#include <windows.h>

int  sw_renderer_init(HWND hwnd);
void sw_draw_quad(int qi);
void sw_display(void);
void sw_renderer_cleanup(void);

#endif
