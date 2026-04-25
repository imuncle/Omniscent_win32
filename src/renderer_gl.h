#ifndef OMNISCENT_RENDERER_GL_H
#define OMNISCENT_RENDERER_GL_H

#include <windows.h>

int  gl_renderer_init(HWND hwnd);
void gl_renderer_draw(void);
void gl_renderer_cleanup(void);

#endif
