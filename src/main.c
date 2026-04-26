#include <windows.h>

#include "omniscent.h"
#include "renderer_sw.h"
#include "renderer_gl.h"
#include "midi.h"

void __stdcall main(void) {
    HINSTANCE hInst = GetModuleHandleA(NULL);
    int ret = WinMain(hInst, NULL, NULL, SW_SHOWDEFAULT);
    ExitProcess(ret);
}

static HWND g_hwnd = NULL;
static omniscent_t g_omni;
static DWORD g_last_tick = 0;
static int g_tick_accum = 0;
static void (*g_clean)(void) = sw_renderer_cleanup;

static void switch_renderer(void) {
    g_clean();
    g_omni.use_gl = !g_omni.use_gl;
    if (g_omni.use_gl) {
        g_clean = gl_renderer_cleanup;
        gl_renderer_init(g_hwnd);
    } else {
        g_clean = sw_renderer_cleanup;
        sw_renderer_init(g_hwnd);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_F1) switch_renderer();
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmdShow) {
    (void)hPrev; (void)lpCmd;

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "OC";
    if (!RegisterClass(&wc)) return 1;

    int scale = 3;
    int border_x = GetSystemMetrics(SM_CXFIXEDFRAME) * 2;
    int border_y = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFIXEDFRAME) * 2;
    int win_w = 320 * scale + border_x;
    int win_h = 200 * scale + border_y;

    g_hwnd = CreateWindow(wc.lpszClassName, "OMNISCENT",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, nCmdShow);

    omniscent_init(&g_omni);
    sw_renderer_init(g_hwnd);
    gl_renderer_init(g_hwnd);

    midi_start();

    g_last_tick = GetTickCount();

    MSG msg;
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        DWORD now = GetTickCount();
        int current_tick = (int)((now - g_last_tick) * 35 / 100);
        while (g_tick_accum < current_tick) {
            if (!omniscent_on_timer(&g_omni)) {
                g_omni.stopping = 1;
                break;
            }
            g_tick_accum++;
        }

        if (g_omni.stopping) break;

        omniscent_render(&g_omni);
        Sleep(1);
    }

done:
    midi_stop();
    g_clean();
    return 0;
}
