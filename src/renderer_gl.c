#include "renderer_gl.h"
#include "renderer_common.h"
#include "model.h"
#include "texture.h"
#include "lightmap.h"
#include "palette.h"
#include <windows.h>
#include <GL/gl.h>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1

typedef void(WINAPI *PFNGLACTIVETEXTUREPROC)(GLenum);

static HGLRC g_gl_hglrc;
static HDC g_gl_hdc;
static HWND g_gl_hwnd;
static GLuint g_color_tex, g_bg_tex, g_lightmap_tex;
static GLuint g_program, g_vbo_pos, g_vbo_tex, g_vbo_light, g_ebo;
static int g_index_count;

typedef GLuint(WINAPI *PFNGLCREATESHADERPROC)(GLenum);
typedef void(WINAPI *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar **, const GLint *);
typedef void(WINAPI *PFNGLCOMPILESHADERPROC)(GLuint);
typedef GLuint(WINAPI *PFNGLCREATEPROGRAMPROC)(void);
typedef void(WINAPI *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void(WINAPI *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void(WINAPI *PFNGLUSEPROGRAMPROC)(GLuint);
typedef GLint(WINAPI *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar *);
typedef void(WINAPI *PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void(WINAPI *PFNGLGENBUFFERSPROC)(GLsizei, GLuint *);
typedef void(WINAPI *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void(WINAPI *PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void(WINAPI *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void(WINAPI *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);

void *gl_procs[15];
#define glCreateShader_p ((PFNGLCREATESHADERPROC)gl_procs[0])
#define glShaderSource_p ((PFNGLSHADERSOURCEPROC)gl_procs[1])
#define glCompileShader_p ((PFNGLCOMPILESHADERPROC)gl_procs[2])
#define glCreateProgram_p ((PFNGLCREATEPROGRAMPROC)gl_procs[3])
#define glAttachShader_p ((PFNGLATTACHSHADERPROC)gl_procs[4])
#define glLinkProgram_p ((PFNGLLINKPROGRAMPROC)gl_procs[5])
#define glUseProgram_p ((PFNGLUSEPROGRAMPROC)gl_procs[6])
#define glGetUniformLocation_p ((PFNGLGETUNIFORMLOCATIONPROC)gl_procs[7])
#define glUniform1i_p ((PFNGLUNIFORM1IPROC)gl_procs[8])
#define glGenBuffers_p ((PFNGLGENBUFFERSPROC)gl_procs[9])
#define glBindBuffer_p ((PFNGLBINDBUFFERPROC)gl_procs[10])
#define glBufferData_p ((PFNGLBUFFERDATAPROC)gl_procs[11])
#define glEnableVertexAttribArray_p ((PFNGLENABLEVERTEXATTRIBARRAYPROC)gl_procs[12])
#define glVertexAttribPointer_p ((PFNGLVERTEXATTRIBPOINTERPROC)gl_procs[13])
#define glActiveTexture_p ((PFNGLACTIVETEXTUREPROC)gl_procs[14])
static const char gl_name_blob[] =
    "glCreateShader\0glShaderSource\0glCompileShader\0"
    "glCreateProgram\0glAttachShader\0glLinkProgram\0"
    "glUseProgram\0glGetUniformLocation\0glUniform1i\0"
    "glGenBuffers\0glBindBuffer\0glBufferData\0"
    "glEnableVertexAttribArray\0glVertexAttribPointer\0glActiveTexture\0";
static int load_gl_functions(void)
{
    const char *p = gl_name_blob;
    for (int i = 0; i < 15; i++)
    {
        gl_procs[i] = wglGetProcAddress(p);
        while (*p++)
            ;
    }
    return glCreateShader_p && glUseProgram_p && glGenBuffers_p;
}

static const char *VS_SRC =
    "attribute vec3 p;attribute vec4 t;attribute float l;varying vec4 v;varying float f;"
    "void main(){gl_Position=gl_ProjectionMatrix*vec4(p,1);v=t;f=l;}";
static const char *FS_SRC =
    "uniform sampler2D c,l;varying vec4 v;varying float f;"
    "void main(){vec2 s=v.xy+v.zw;float i=texture2D(c,s).r;"
    "if(i==.0)discard;"
    "gl_FragColor=texture2D(l,vec2(.996*i+.002,floor(f/256.)/128.+.004));}";

static uint32_t conv_buf[FB_SIZE];
static void rgba_convert(const uint8_t *src, int n)
{
    for (int i = 0; i < n; i++)
    {
        uint8_t *p = &g_palette[src[i] * 3];
        conv_buf[i] = p[0] | (p[1] << 8) | (p[2] << 16) | 0xFF000000;
    }
}

int gl_renderer_init(HWND hwnd)
{
    g_gl_hwnd = hwnd;
    g_gl_hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0};
    int pf = ChoosePixelFormat(g_gl_hdc, &pfd);
    if (!pf)
    {
        ReleaseDC(hwnd, g_gl_hdc);
        return 0;
    }
    SetPixelFormat(g_gl_hdc, pf, &pfd);

    g_gl_hglrc = wglCreateContext(g_gl_hdc);
    if (!g_gl_hglrc)
    {
        ReleaseDC(hwnd, g_gl_hdc);
        return 0;
    }
    wglMakeCurrent(g_gl_hdc, g_gl_hglrc);

    if (!load_gl_functions())
    {
        wglDeleteContext(g_gl_hglrc);
        ReleaseDC(hwnd, g_gl_hdc);
        return 0;
    }

    GLuint vs = glCreateShader_p(GL_VERTEX_SHADER);
    glShaderSource_p(vs, 1, &VS_SRC, NULL);
    glCompileShader_p(vs);
    GLuint fs = glCreateShader_p(GL_FRAGMENT_SHADER);
    glShaderSource_p(fs, 1, &FS_SRC, NULL);
    glCompileShader_p(fs);
    g_program = glCreateProgram_p();
    glAttachShader_p(g_program, vs);
    glAttachShader_p(g_program, fs);
    glLinkProgram_p(g_program);

    static uint8_t atlas[256 * 256];
    for (int i = 0; i < 15; i++)
    {
        for (int j = 0; j < 4096; j++)
        {
            int x = (i & 3) * 64 + (j & 63);
            int y = (i >> 2) * 64 + (j >> 6);
            atlas[y * 256 + x] = g_textures[i][j];
        }
    }
    glGenTextures(1, &g_color_tex);
    glBindTexture(GL_TEXTURE_2D, g_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &g_bg_tex);
    glBindTexture(GL_TEXTURE_2D, g_bg_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    rgba_convert(g_bg, FB_SIZE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FB_W, FB_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv_buf);

    glGenTextures(1, &g_lightmap_tex);
    glBindTexture(GL_TEXTURE_2D, g_lightmap_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenBuffers_p(1, &g_vbo_pos);
    glGenBuffers_p(1, &g_vbo_tex);
    glGenBuffers_p(1, &g_vbo_light);
    glGenBuffers_p(1, &g_ebo);

    return 1;
}

static void gl_build_buffers(void)
{
    static float pos_data[MAX_QUAD * 4 * 3];
    static float tex_data[MAX_QUAD * 4 * 4];
    static uint16_t light_data[MAX_QUAD * 4];
    static uint16_t idx_data[MAX_QUAD * 6];

    float tux = 0.25f, tuy = 0.25f, ts = 1.0f / 256.0f;
    int vtx_count = 0;
    int idx_ptr = 0;

    for (int i = 0; i < g_quad_count; i++)
    {
        int qi = g_sort_list[i * 2 + 1];
        int clipped = 0;
        for (int j = 0; j < 4; j++)
        {
            if (g_tvtxs[g_quads[qi].idx[j] * 3 + 2] + 512 < 0)
            {
                clipped = 1;
                break;
            }
        }
        if (clipped)
            continue;

        int tex_idx = g_quads[qi].idx[4];
        float tx0 = (tex_idx & 3) * tux;
        float ty0 = (tex_idx >> 2) * tuy;
        int base = vtx_count;

        static const float U_MAP[4] = {0.1f, 0.1f, 63.1f, 63.1f};
        static const float V_MAP[4] = {63.1f, 0.1f, 0.1f, 63.1f};

        for (int j = 0; j < 4; j++)
        {
            int vi = g_quads[qi].idx[j];
            pos_data[vtx_count * 3 + 0] = g_tvtxs[vi * 3 + 0];
            pos_data[vtx_count * 3 + 1] = g_tvtxs[vi * 3 + 1];
            pos_data[vtx_count * 3 + 2] = g_tvtxs[vi * 3 + 2];

            int z = (int)g_tvtxs[vi * 3 + 2] + 512;
            if (z < 0)
                z = 0;
            if (z > 511)
                z = 511;
            light_data[vtx_count] = (uint16_t)(((z << 7) * g_vtxs[vi].v[3]) >> 16);

            tex_data[vtx_count * 4 + 0] = tx0;
            tex_data[vtx_count * 4 + 1] = ty0;
            tex_data[vtx_count * 4 + 2] = U_MAP[j] * ts;
            tex_data[vtx_count * 4 + 3] = V_MAP[j] * ts;
            vtx_count++;
        }
        idx_data[idx_ptr++] = base + 0;
        idx_data[idx_ptr++] = base + 1;
        idx_data[idx_ptr++] = base + 2;
        idx_data[idx_ptr++] = base + 0;
        idx_data[idx_ptr++] = base + 2;
        idx_data[idx_ptr++] = base + 3;
    }
    g_index_count = idx_ptr;

    glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_pos);
    glBufferData_p(GL_ARRAY_BUFFER, vtx_count * 12, pos_data, GL_DYNAMIC_DRAW);
    glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_tex);
    glBufferData_p(GL_ARRAY_BUFFER, vtx_count * 16, tex_data, GL_DYNAMIC_DRAW);
    glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_light);
    glBufferData_p(GL_ARRAY_BUFFER, vtx_count * 2, light_data, GL_DYNAMIC_DRAW);
    glBindBuffer_p(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData_p(GL_ELEMENT_ARRAY_BUFFER, idx_ptr * 2, idx_data, GL_DYNAMIC_DRAW);
}

void gl_renderer_draw(void)
{
    if (!g_program)
        return;
    RECT rc;
    GetClientRect(g_gl_hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    rgba_convert(g_lightmap, 32768);
    glActiveTexture_p(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_lightmap_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 128, GL_RGBA, GL_UNSIGNED_BYTE, conv_buf);

    static uint32_t bg_rgba[FB_SIZE];
    for (int i = 0; i < FB_SIZE; i++)
    {
        uint8_t *p = &g_palette[g_bg[i] * 3];
        bg_rgba[i] = p[0] | (p[1] << 8) | (p[2] << 16) | 0xFF000000;
    }
    glActiveTexture_p(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_bg_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FB_W, FB_H, GL_RGBA, GL_UNSIGNED_BYTE, bg_rgba);

    glViewport(0, 0, w, h);
    glUseProgram_p(0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, 1);
    glTexCoord2f(1, 0);
    glVertex2f(1, 1);
    glTexCoord2f(1, 1);
    glVertex2f(1, -1);
    glTexCoord2f(0, 1);
    glVertex2f(-1, -1);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, g_color_tex);
    {
        static const uint8_t ti[] = {1, 0x0D};
        for (int k = 0; k < 2; k++)
        {
            uint8_t i = ti[k];
            int xoffset = (i & 3) * 64;
            int yoffset = (i >> 2) * 64;
            glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, 64, 64, GL_LUMINANCE, GL_UNSIGNED_BYTE, g_textures[i]);
        }
    }
    gl_build_buffers();
    if (g_index_count > 0)
    {
        glUseProgram_p(g_program);
        glActiveTexture_p(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_color_tex);
        glUniform1i_p(glGetUniformLocation_p(g_program, "c"), 0);
        glActiveTexture_p(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_lightmap_tex);
        glUniform1i_p(glGetUniformLocation_p(g_program, "l"), 1);

        static const float proj[16] = {
            0.0075f, 0, 0, 0,
            0, -0.012578f, 0, 0,
            0, 0, 0, -0.00625f,
            0, 0, -0.001953f, 0.08125f};

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glViewport(0, h / 10 - 1, w, h * 159 / 200);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_pos);
        glEnableVertexAttribArray_p(0);
        glVertexAttribPointer_p(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_tex);
        glEnableVertexAttribArray_p(1);
        glVertexAttribPointer_p(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer_p(GL_ARRAY_BUFFER, g_vbo_light);
        glEnableVertexAttribArray_p(2);
        glVertexAttribPointer_p(2, 1, GL_UNSIGNED_SHORT, GL_FALSE, 0, 0);
        glBindBuffer_p(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
        glDrawElements(GL_TRIANGLES, g_index_count, GL_UNSIGNED_SHORT, 0);
        glUseProgram_p(0);
    }

    SwapBuffers(g_gl_hdc);
}

void gl_renderer_cleanup(void)
{
    if (g_gl_hglrc)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_gl_hglrc);
    }
    if (g_gl_hdc)
        ReleaseDC(g_gl_hwnd, g_gl_hdc);
}
