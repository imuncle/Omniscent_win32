#include "omniscent.h"
#include "random.h"
#include "palette.h"
#include "lightmap.h"
#include "texture.h"
#include "model.h"
#include "camera.h"
#include "renderer_common.h"
#include "renderer_sw.h"
#include "renderer_gl.h"
#include "util.h"
#include "midi.h"

static camera_t g_cam;

void omniscent_init(omniscent_t *o) {
    o->stopping = 0;
    o->use_gl = 0;
    o->renderer_counter = 0;
    o->bg_star_drawn = 0;

    palette_generate();
    lightmap_generate();
    textures_generate();
    model_generate();
    cam_init(&g_cam);
    generate_text();
    midi_init();
}

int omniscent_on_timer(omniscent_t *o) {
    midi_tick();

    o->renderer_counter = (o->renderer_counter + 1) & 0x7;
    if (o->renderer_counter == 0) {
        palette_tick();
        textures_tick();
    }

    return cam_tick(&g_cam);
}

void omniscent_render(omniscent_t *o) {
    if (textures_is_door_opened() && !o->bg_star_drawn) {
        o->bg_star_drawn = 1;
        for (int i = 0; i < 100; i++) {
            int p = rnd_next(0xC1C0) + 0x1900;
            draw_star(g_bg, p, 0x013B, 0x0002, (p >> 8) & 0x07);
        }
    }

    textures_update_door();

    const int y0 = 18, y1 = 179;
    for (int y = y0; y <= y1; y++)
        for (int x = 0; x < 320; x++)
            g_fb[y * 320 + x] = g_bg[y * 320 + x];

    model_transform(cam_get_pos(&g_cam), cam_get_matrix(&g_cam));

    renderer_sort_quads();

    if (!o->use_gl) {
        for (int i = 0; i < g_quad_count; i++)
            sw_draw_quad(g_sort_list[i * 2 + 1]);
        sw_display();
    } else {
        gl_renderer_draw();
    }
}
