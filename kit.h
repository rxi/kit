// kit v0.3 | public domain - no warranty implied; use at your own risk

#ifndef KIT_H
#define KIT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>
#include <windows.h>
#include <windowsx.h>

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#endif

#ifdef BUILD_DLL
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

enum {
    KIT_SCALE2X    = (1 << 0),
    KIT_SCALE3X    = (1 << 1),
    KIT_SCALE4X    = (1 << 2),
    KIT_HIDECURSOR = (1 << 3),
    KIT_FPS30      = (1 << 4),
    KIT_FPS144     = (1 << 5),
    KIT_FPSINF     = (1 << 6),
};

typedef union { struct { uint8_t b, g, r, a; }; uint32_t w; } kit_Color;
typedef struct { int x, y, w, h; } kit_Rect;
typedef struct { kit_Color *pixels; int w, h; } kit_Image;
typedef struct { kit_Rect rect; int xadv; } kit_Glyph;
typedef struct { kit_Image *image; kit_Glyph glyphs[256]; } kit_Font;

typedef struct {
    bool wants_quit;
    bool hide_cursor;
    // input
    int char_buf[32];
    uint8_t key_state[256];
    uint8_t mouse_state[16];
    struct { int x, y; } mouse_pos;
    struct { int x, y; } mouse_delta;
    // time
    double step_time;
    double prev_time;
    // graphics
    kit_Rect clip;
    kit_Font *font;
    kit_Image *screen;
    // windows
    int win_w, win_h;
    HWND hwnd;
    HDC hdc;
} kit_Context;

#define kit_max(a, b) ((a) > (b) ? (a) : (b))
#define kit_min(a, b) ((a) < (b) ? (a) : (b))
#define kit_lengthof(a) (sizeof(a) / sizeof((a)[0]))

#define kit_rect(X, Y, W, H) ((kit_Rect) { (X), (Y), (W), (H) })
#define kit_rgba(R, G, B, A) ((kit_Color) { .r = (R), .g = (G), .b = (B), .a = (A) })
#define kit_rgb(R, G, B) kit_rgba(R, G, B, 0xff)
#define kit_alpha(A) kit_rgba(0xff, 0xff, 0xff, A)

#define KIT_BIG_RECT kit_rect(0, 0, 0xffffff, 0xffffff)
#define KIT_WHITE    kit_rgb(0xff, 0xff, 0xff)
#define KIT_BLACK    kit_rgb(0, 0, 0)

DLL_EXPORT kit_Context *kit_create(const char *title, int w, int h, int flags);
DLL_EXPORT void kit_destroy(kit_Context *ctx);
DLL_EXPORT bool kit_step(kit_Context *ctx, double *dt);
DLL_EXPORT void *kit_read_file(char *filename, int *len);

DLL_EXPORT kit_Image *kit_create_image(int w, int h);
DLL_EXPORT kit_Image *kit_load_image_file(char *filename);
DLL_EXPORT kit_Image *kit_load_image_mem(void *data, int len);
DLL_EXPORT void kit_destroy_image(kit_Image *img);

DLL_EXPORT kit_Font *kit_load_font_file(char *filename);
DLL_EXPORT kit_Font *kit_load_font_mem(void *data, int len);
DLL_EXPORT void kit_destroy_font(kit_Font *font);
DLL_EXPORT int kit_text_width(kit_Font *font, char *text);

DLL_EXPORT int kit_get_char(kit_Context *ctx);
DLL_EXPORT bool kit_key_down(kit_Context *ctx, int key);
DLL_EXPORT bool kit_key_pressed(kit_Context *ctx, int key);
DLL_EXPORT bool kit_key_released(kit_Context *ctx, int key);
DLL_EXPORT void kit_mouse_pos(kit_Context *ctx, int *x, int *y);
DLL_EXPORT void kit_mouse_delta(kit_Context *ctx, int *x, int *y);
DLL_EXPORT bool kit_mouse_down(kit_Context *ctx, int button);
DLL_EXPORT bool kit_mouse_pressed(kit_Context *ctx, int button);
DLL_EXPORT bool kit_mouse_released(kit_Context *ctx, int button);

DLL_EXPORT void kit_clear(kit_Context *ctx, kit_Color color);
DLL_EXPORT void kit_set_clip(kit_Context *ctx, kit_Rect rect);
DLL_EXPORT void kit_draw_point(kit_Context *ctx, kit_Color color, int x, int y);
DLL_EXPORT void kit_draw_rect(kit_Context *ctx, kit_Color color, kit_Rect rect);
DLL_EXPORT void kit_draw_line(kit_Context *ctx, kit_Color color, int x1, int y1, int x2, int y2);
DLL_EXPORT void kit_draw_image(kit_Context *ctx, kit_Image *img, int x, int y);
DLL_EXPORT void kit_draw_image2(kit_Context *ctx, kit_Color color, kit_Image *img, int x, int y, kit_Rect src);
DLL_EXPORT void kit_draw_image3(kit_Context *ctx, kit_Color mul_color, kit_Color add_color, kit_Image *img, kit_Rect dst, kit_Rect src);
DLL_EXPORT int kit_draw_text(kit_Context *ctx, kit_Color color, char *text, int x, int y);
DLL_EXPORT int kit_draw_text2(kit_Context *ctx, kit_Color color, kit_Font *font, char *text, int x, int y);

#endif // KIT_H

//////////////////////////////////////////////////////////////////////////////

#ifdef KIT_IMPL

enum {
    KIT_INPUT_DOWN     = (1 << 0),
    KIT_INPUT_PRESSED  = (1 << 1),
    KIT_INPUT_RELEASED = (1 << 2),
};

#define kit__expect(x) if (!(x)) { kit__panic("assertion failure: %s", #x); }

static void kit__panic(char *fmt, ...) {
    fprintf(stderr, "kit fatal error: ");
    va_list ap;
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}


static void* kit__alloc(int n) {
    void *res = calloc(1, n);
    if (!res) { kit__panic("out of memory"); }
    return res;
}


static bool kit__check_input_flag(uint8_t *t, uint32_t idx, uint32_t cap, int flag) {
    if (idx > cap) { return false; }
    return t[idx] & flag ? true : false;
}


static void kit__scale_size_by_flags(int *w, int *h, int flags) {
    if (flags & KIT_SCALE2X) { *w *= 2; *h *= 2; } else
    if (flags & KIT_SCALE3X) { *w *= 3; *h *= 3; } else
    if (flags & KIT_SCALE4X) { *w *= 4; *h *= 4; }
}


static double kit__flags_to_step_time(int flags) {
    if (flags & KIT_FPS30 ) { return 1.0 /  30.0; }
    if (flags & KIT_FPS144) { return 1.0 / 144.0; }
    if (flags & KIT_FPSINF) { return 0; }
    return 1.0 / 60.0;
}

static double kit__now(void) {
    return clock() / 1000.0;
}


static kit_Rect kit__intersect_rects(kit_Rect a, kit_Rect b) {
    int x1 = kit_max(a.x, b.x);
    int y1 = kit_max(a.y, b.y);
    int x2 = kit_min(a.x + a.w, b.x + b.w);
    int y2 = kit_min(a.y + a.h, b.y + b.h);
    return (kit_Rect) { x1, y1, x2 - x1, y2 - y1 };
}


static inline kit_Color kit__blend_pixel(kit_Color dst, kit_Color src) {
    kit_Color res;
    res.w = (dst.w & 0xff00ff) + ((((src.w & 0xff00ff) - (dst.w & 0xff00ff)) * src.a) >> 8);
    res.g = dst.g + (((src.g - dst.g) * src.a) >> 8);
    res.a = dst.a;
    return res;
}


static inline kit_Color kit__blend_pixel2(kit_Color dst, kit_Color src, kit_Color clr) {
    src.a = (src.a * clr.a) >> 8;
    int ia = 0xff - src.a;
    dst.r = ((src.r * clr.r * src.a) >> 16) + ((dst.r * ia) >> 8);
    dst.g = ((src.g * clr.g * src.a) >> 16) + ((dst.g * ia) >> 8);
    dst.b = ((src.b * clr.b * src.a) >> 16) + ((dst.b * ia) >> 8);
    return dst;
}


static inline kit_Color kit__blend_pixel3(kit_Color dst, kit_Color src, kit_Color clr, kit_Color add) {
  src.r = kit_min(255, src.r + add.r);
  src.g = kit_min(255, src.g + add.g);
  src.b = kit_min(255, src.b + add.b);
  return kit__blend_pixel2(dst, src, clr);
}


static kit_Rect kit__get_adjusted_window_rect(kit_Context *ctx) {
    // work out maximum size to retain aspect ratio
    float src_ar = (float) ctx->screen->h / ctx->screen->w;
    float dst_ar = (float) ctx->win_h / ctx->win_w;
    int w, h;
    if (src_ar < dst_ar) {
        w = ctx->win_w; h = ceil(w * src_ar);
    } else {
        h = ctx->win_h; w = ceil(h / src_ar);
    }
    // return centered rect
    return kit_rect((ctx->win_w - w) / 2, (ctx->win_h - h) / 2, w, h);
}


static LRESULT CALLBACK kit__wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    kit_Context *ctx = (void*) GetProp(hWnd, "kit_Context");

    switch (message) {
    case WM_PAINT:;
        BITMAPINFO bmi = {
            .bmiHeader.biSize = sizeof(BITMAPINFOHEADER),
            .bmiHeader.biBitCount = 32,
            .bmiHeader.biCompression = BI_RGB,
            .bmiHeader.biPlanes = 1,
            .bmiHeader.biWidth = ctx->screen->w,
            .bmiHeader.biHeight = -ctx->screen->h
        };

        kit_Rect wr = kit__get_adjusted_window_rect(ctx);

        StretchDIBits(ctx->hdc,
            wr.x, wr.y, wr.w, wr.h,
            0, 0, ctx->screen->w, ctx->screen->h,
            ctx->screen->pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

        ValidateRect(hWnd, 0);
        break;

    case WM_SETCURSOR:
        if (ctx->hide_cursor && LOWORD(lParam) == HTCLIENT) {
            SetCursor(0);
            break;
        }
        goto unhandled;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (lParam & (1 << 30)) { // key repeat
            break;
        }
        ctx->key_state[(uint8_t) wParam] = KIT_INPUT_DOWN | KIT_INPUT_PRESSED;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        ctx->key_state[(uint8_t) wParam] &= ~KIT_INPUT_DOWN;
        ctx->key_state[(uint8_t) wParam] |=  KIT_INPUT_RELEASED;
        break;

    case WM_CHAR:
        if (wParam < 32) { break; }
        for (int i = 0; i < kit_lengthof(ctx->char_buf); i++) {
            if (ctx->char_buf[i]) { continue; }
            ctx->char_buf[i] = wParam;
            break;
        }
        break;

    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:;
        int button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) ? 1 :
                     (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? 2 : 3;
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN) {
            SetCapture(hWnd);
            ctx->mouse_state[button] = KIT_INPUT_DOWN | KIT_INPUT_PRESSED;
        } else {
            ReleaseCapture();
            ctx->mouse_state[button] &= ~KIT_INPUT_DOWN;
            ctx->mouse_state[button] |=  KIT_INPUT_RELEASED;
        }
        // fallthrough

    case WM_MOUSEMOVE:;
        wr = kit__get_adjusted_window_rect(ctx);
        int prevx = ctx->mouse_pos.x;
        int prevy = ctx->mouse_pos.y;
        ctx->mouse_pos.x = (GET_X_LPARAM(lParam) - wr.x) * ctx->screen->w / wr.w;
        ctx->mouse_pos.y = (GET_Y_LPARAM(lParam) - wr.y) * ctx->screen->h / wr.h;
        ctx->mouse_delta.x += ctx->mouse_pos.x - prevx;
        ctx->mouse_delta.y += ctx->mouse_pos.y - prevy;
        break;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            // set size
            ctx->win_w = LOWORD(lParam);
            ctx->win_h = HIWORD(lParam);
            // paint window black
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush);
            EndPaint(hWnd, &ps);
            // redraw
            RedrawWindow(ctx->hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);
        }
        break;

    case WM_QUIT:
    case WM_CLOSE:
        ctx->wants_quit = true;
        break;

    default:
unhandled:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


static void *kit__font_png_data;
static int   kit__font_png_size;

kit_Context* kit_create(const char *title, int w, int h, int flags) {
    kit_Context *ctx = kit__alloc(sizeof(kit_Context));

    ctx->screen = kit_create_image(w, h);
    ctx->step_time = kit__flags_to_step_time(flags);
    ctx->hide_cursor = !!(flags & KIT_HIDECURSOR);
    ctx->clip = kit_rect(0, 0, w, h);

    RegisterClass(&(WNDCLASS) {
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = kit__wndproc,
        .hCursor = LoadCursor(0, IDC_ARROW),
        .lpszClassName = title,
        .hIcon = LoadIcon(GetModuleHandle(0), "icon"),
    });

    kit__scale_size_by_flags(&w, &h, flags);
    RECT rect = { .right = w, .bottom = h };
    int style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rect, style, 0);
    ctx->hwnd = CreateWindow(
        title, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        0, 0, 0, 0
    );
    SetProp(ctx->hwnd, "kit_Context", ctx);

    ShowWindow(ctx->hwnd, SW_NORMAL);
    ctx->hdc = GetDC(ctx->hwnd);

    timeBeginPeriod(1);

    ctx->font = kit_load_font_mem(kit__font_png_data, kit__font_png_size);
    ctx->prev_time = kit__now();

    return ctx;
}


void kit_destroy(kit_Context *ctx) {
    ReleaseDC(ctx->hwnd, ctx->hdc);
    DestroyWindow(ctx->hwnd);
    kit_destroy_image(ctx->screen);
    kit_destroy_font(ctx->font);
    free(ctx);
}


int kit_text_width(kit_Font *font, char *text) {
    int x = 0;
    for (uint8_t *p = (void*) text; *p; p++) {
        x += font->glyphs[*p].xadv;
    }
    return x;
}


bool kit_step(kit_Context *ctx, double *dt) {
    // present
    RedrawWindow(ctx->hwnd, 0, 0, RDW_INVALIDATE | RDW_UPDATENOW);

    // handle delta time / wait for next frame
    double now = kit__now();
    double wait = (ctx->prev_time + ctx->step_time) - now;
    double prev = ctx->prev_time;
    if (wait > 0) {
        Sleep(wait * 1000);
        ctx->prev_time += ctx->step_time;
    } else {
        ctx->prev_time = now;
    }
    if (dt) { *dt = ctx->prev_time - prev; }

    // reset input state
    memset(ctx->char_buf, 0, sizeof(ctx->char_buf));
    for (int i = 0; i < sizeof(ctx->key_state); i++) {
        ctx->key_state[i] &= ~(KIT_INPUT_PRESSED | KIT_INPUT_RELEASED);
    }
    for (int i = 0; i < sizeof(ctx->mouse_state); i++) {
        ctx->mouse_state[i] &= ~(KIT_INPUT_PRESSED | KIT_INPUT_RELEASED);
    }
    memset(&ctx->mouse_delta, 0, sizeof(ctx->mouse_delta));

    // handle events
    MSG msg;
    while (PeekMessage(&msg, ctx->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !ctx->wants_quit;
}


void* kit_read_file(char *filename, int *len) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) { return NULL; }
    fseek(fp, 0, SEEK_END);
    int n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = kit__alloc(n + 1);
    fread(buf, 1, n, fp);
    fclose(fp);
    if (len) { *len = n; }
    return buf;
}


kit_Image* kit_create_image(int w, int h) {
    kit__expect(w > 0 && h > 0);
    kit_Image *img = kit__alloc(sizeof(kit_Image) + w * h * sizeof(kit_Color));
    img->pixels = (void*) (img + 1);
    img->w = w;
    img->h = h;
    return img;
}


kit_Image* kit_load_image_file(char *filename) {
    int len;
    void *data = kit_read_file(filename, &len);
    if (!data) { return NULL; }
    kit_Image *res = kit_load_image_mem(data, len);
    free(data);
    return res;
}


static kit_Image* kit__load_png(void *data, int len);

kit_Image* kit_load_image_mem(void *data, int len) {
    return kit__load_png(data, len);
}


void kit_destroy_image(kit_Image *img) {
    free(img);
}


static bool kit__check_column(kit_Image *img, int x, int y, int h) {
    while (h > 0) {
        if (img->pixels[x + y * img->w].a) {
            return true;
        }
        y++; h--;
    }
    return false;
}


static kit_Font* kit__load_font_from_image(kit_Image *img) {
    if (!img) { return NULL; }
    kit_Font *font = kit__alloc(sizeof(kit_Font));
    font->image = img;

    // init glyphs
    for (int i = 0; i < 256; i++) {
        kit_Glyph *g = &font->glyphs[i];
        kit_Rect r = {
            (img->w / 16) * (i % 16),
            (img->h / 16) * (i / 16),
            img->w / 16,
            img->h / 16
        };
        // right-trim rect
        for (int x = r.x + r.w - 1; x >= r.x; x--) {
            if (kit__check_column(font->image, x, r.y, r.h)) { break; }
            r.w--;
        }
        // left-trim rect
        for (int x = r.x; x < r.x + r.w; x++) {
            if (kit__check_column(font->image, x, r.y, r.h)) { break; }
            r.x++;
            r.w--;
        }
        // set xadvance and rect
        g->xadv = r.w + 1;
        g->rect = r;
    }

    font->glyphs[' '].rect = (kit_Rect) {0};
    font->glyphs[' '].xadv = font->glyphs['a'].xadv;

    return font;
}


kit_Font* kit_load_font_file(char *filename) {
    return kit__load_font_from_image(kit_load_image_file(filename));
}


kit_Font* kit_load_font_mem(void *data, int len) {
    return kit__load_font_from_image(kit_load_image_mem(data, len));
}


void kit_destroy_font(kit_Font *font) {
    free(font->image);
    free(font);
}


int kit_get_char(kit_Context *ctx) {
    for (int i = 0; i < kit_lengthof(ctx->char_buf); i++) {
        if (!ctx->char_buf[i]) { continue; }
        int res = ctx->char_buf[i];
        ctx->char_buf[i] = 0;
        return res;
    }
    return 0;
}


bool kit_key_down(kit_Context *ctx, int key) {
    return kit__check_input_flag(ctx->key_state, key, sizeof(ctx->key_state), KIT_INPUT_DOWN);
}


bool kit_key_pressed(kit_Context *ctx, int key) {
    return kit__check_input_flag(ctx->key_state, key, sizeof(ctx->key_state), KIT_INPUT_PRESSED);
}


bool kit_key_released(kit_Context *ctx, int key) {
    return kit__check_input_flag(ctx->key_state, key, sizeof(ctx->key_state), KIT_INPUT_RELEASED);
}


void kit_mouse_pos(kit_Context *ctx, int *x, int *y) {
    if (x) { *x = ctx->mouse_pos.x; }
    if (y) { *y = ctx->mouse_pos.y; }
}


void kit_mouse_delta(kit_Context *ctx, int *x, int *y) {
    if (x) { *x = ctx->mouse_delta.x; }
    if (y) { *y = ctx->mouse_delta.y; }
}


bool kit_mouse_down(kit_Context *ctx, int button) {
    return kit__check_input_flag(ctx->mouse_state, button, sizeof(ctx->mouse_state), KIT_INPUT_DOWN);
}


bool kit_mouse_pressed(kit_Context *ctx, int button) {
    return kit__check_input_flag(ctx->mouse_state, button, sizeof(ctx->mouse_state), KIT_INPUT_PRESSED);
}


bool kit_mouse_released(kit_Context *ctx, int button) {
    return kit__check_input_flag(ctx->mouse_state, button, sizeof(ctx->mouse_state), KIT_INPUT_RELEASED);
}


void kit_clear(kit_Context *ctx, kit_Color color) {
    kit_draw_rect(ctx, color, KIT_BIG_RECT);
}


void kit_set_clip(kit_Context *ctx, kit_Rect rect) {
    kit_Rect screen_rect = kit_rect(0, 0, ctx->screen->w, ctx->screen->h);
    ctx->clip = kit__intersect_rects(rect, screen_rect);
}


void kit_draw_point(kit_Context *ctx, kit_Color color, int x, int y) {
    if (color.a == 0) { return; }
    kit_Rect r = ctx->clip;
    if (x < r.x || y < r.y || x >= r.x + r.w || y >= r.y + r.h ) {
        return;
    }
    kit_Color *dst = &ctx->screen->pixels[x + y * ctx->screen->w];
    *dst = kit__blend_pixel(*dst, color);
}


void kit_draw_rect(kit_Context *ctx, kit_Color color, kit_Rect rect) {
    if (color.a == 0) { return; }
    rect = kit__intersect_rects(rect, ctx->clip);
    kit_Color *d = &ctx->screen->pixels[rect.x + rect.y * ctx->screen->w];
    int dr = ctx->screen->w - rect.w;
    for (int y = 0; y < rect.h; y++) {
        for (int x = 0; x < rect.w; x++) {
            *d = kit__blend_pixel(*d, color);
            d++;
        }
        d += dr;
    }
}


void kit_draw_line(kit_Context *ctx, kit_Color color, int x1, int y1, int x2, int y2) {
    int dx = abs(x2-x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        kit_draw_point(ctx, color, x1, y1);
        if (x1 == x2 && y1 == y2) { break; }
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}


void kit_draw_image(kit_Context *ctx, kit_Image *img, int x, int y) {
    kit_Rect dst = kit_rect(x, y, img->w, img->h);
    kit_Rect src = kit_rect(0, 0, img->w, img->h);
    kit_draw_image3(ctx, KIT_WHITE, KIT_BLACK, img, dst, src);
}


void kit_draw_image2(kit_Context *ctx, kit_Color color, kit_Image *img, int x, int y, kit_Rect src) {
    kit_Rect dst = kit_rect(x, y, abs(src.w), abs(src.h));
    kit_draw_image3(ctx, color, KIT_BLACK, img, dst, src);
}


void kit_draw_image3(kit_Context *ctx, kit_Color mul_color, kit_Color add_color, kit_Image *img, kit_Rect dst, kit_Rect src) {
    // early exit on zero-sized anything
    if (!src.w || !src.w || !dst.w || !dst.h) {
        return;
    }

    /* do scaled render */
    int cx1 = ctx->clip.x;
    int cy1 = ctx->clip.y;
    int cx2 = cx1 + ctx->clip.w;
    int cy2 = cy1 + ctx->clip.h;
    int stepx = (src.w << 10) / dst.w;
    int stepy = (src.h << 10) / dst.h;
    int sy = src.y << 10;

    /* vertical clipping */
    int dy = dst.y;
    if (dy < cy1) { sy += (cy1 - dy) * stepy; dy = cy1; }
    int ey = kit_min(cy2, dst.y + dst.h);

    int blend_fn = 1;
    if (mul_color.w != 0xffffffff) { blend_fn = 2; }
    if ((add_color.w & 0xffffff00) != 0xffffff00) { blend_fn = 3; }

    for (; dy < ey; dy++) {
        if (dy >= cy1 && dy < cy2) {
            int sx = src.x << 10;
            kit_Color *srow = &img->pixels[(sy >> 10) * img->w];
            kit_Color *drow = &ctx->screen->pixels[dy * ctx->screen->w];

            /* horizontal clipping */
            int dx = dst.x;
            if (dx < cx1) { sx += (cx1 - dx) * stepx; dx = cx1; }
            int ex = kit_min(cx2, dst.x + dst.w);

            for (; dx < ex; dx++) {
                kit_Color *s = &srow[sx >> 10];
                kit_Color *d = &drow[dx];
                switch (blend_fn) {
                case 1: *d = kit__blend_pixel (*d, *s); break;
                case 2: *d = kit__blend_pixel2(*d, *s, mul_color); break;
                case 3: *d = kit__blend_pixel3(*d, *s, mul_color, add_color); break;
                }
                sx += stepx;
            }
        }
        sy += stepy;
    }
}


int kit_draw_text(kit_Context *ctx, kit_Color color, char *text, int x, int y) {
    return kit_draw_text2(ctx, color, ctx->font, text, x, y);
}


int kit_draw_text2(kit_Context *ctx, kit_Color color, kit_Font *font, char *text, int x, int y) {
    for (uint8_t *p = (void*) text; *p; p++) {
        kit_Glyph g = font->glyphs[*p];
        kit_draw_image2(ctx, color, font->image, x, y, g.rect);
        x += g.xadv;
    }
    return x;
}


//////////////////////////////////////////////////////////////////////////////
// PNG loader | borrowed from tigr : https://github.com/erkkah/tigr
//////////////////////////////////////////////////////////////////////////////

typedef struct {
    const unsigned char *p, *end;
} kit__Png;

typedef struct {
    unsigned bits, count;
    const unsigned char *in, *inend;
    unsigned char *out, *outend;
    jmp_buf jmp;
    unsigned litcodes[288], distcodes[32], lencodes[19];
    int tlit, tdist, tlen;
} kit__PngState;


#define FAIL() longjmp(s->jmp, 1)
#define CHECK(X) if (!(X)) { FAIL(); }

// Built-in DEFLATE standard tables.
static char    kit__png_order[]        = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
static char  kit__png_len_bits[29 + 2] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0 };
static int   kit__png_len_base[29 + 2] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0 };
static char kit__png_dist_bits[30 + 2] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0 };
static int  kit__png_dist_base[30 + 2] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };

// Table to bit-reverse a byte.
static const unsigned char kit__png_reverse_table[256] = {
#define R2(n) n, n + 128, n + 64, n + 192
#define R4(n) R2(n), R2(n + 32), R2(n + 16), R2(n + 48)
#define R6(n) R4(n), R4(n + 8), R4(n + 4), R4(n + 12)
    R6(0), R6(2), R6(1), R6(3)
};
#undef R2
#undef R4
#undef R6

static unsigned kit__png_rev16(unsigned n) {
    return (kit__png_reverse_table[n & 0xff] << 8) | kit__png_reverse_table[(n >> 8) & 0xff];
}

static int kit__png_bits(kit__PngState* s, int n) {
    int v = s->bits & ((1 << n) - 1);
    s->bits >>= n;
    s->count -= n;
    while (s->count < 16) {
        CHECK(s->in != s->inend);
        s->bits |= (*s->in++) << s->count;
        s->count += 8;
    }
    return v;
}

static unsigned char* kit__png_emit(kit__PngState* s, int len) {
    s->out += len;
    CHECK(s->out <= s->outend);
    return s->out - len;
}

static void kit__png_copy(kit__PngState* s, const unsigned char* src, int len) {
    unsigned char* dest = kit__png_emit(s, len);
    while (len--) { *dest++ = *src++; }
}

static int kit__png_build(kit__PngState* s, unsigned* tree, unsigned char* lens, int symcount) {
    int n, codes[16], first[16], counts[16] = { 0 };

    // Frequency count.
    for (n = 0; n < symcount; n++)
        counts[lens[n]]++;

    // Distribute codes.
    counts[0] = codes[0] = first[0] = 0;
    for (n = 1; n <= 15; n++) {
        codes[n] = (codes[n - 1] + counts[n - 1]) << 1;
        first[n] = first[n - 1] + counts[n - 1];
    }
    CHECK(first[15] + counts[15] <= symcount);

    // Insert keys into the tree for each symbol.
    for (n = 0; n < symcount; n++) {
        int len = lens[n];
        if (len != 0) {
            int code = codes[len]++, slot = first[len]++;
            tree[slot] = (code << (32 - len)) | (n << 4) | len;
        }
    }

    return first[15];
}

static int kit__png_decode(kit__PngState* s, unsigned tree[], int max) {
    // Find the next prefix code.
    unsigned lo = 0, hi = max, key;
    unsigned search = (kit__png_rev16(s->bits) << 16) | 0xffff;
    while (lo < hi) {
        unsigned guess = (lo + hi) / 2;
        if (search < tree[guess]) {
            hi = guess;
        } else {
            lo = guess + 1;
        }
    }

    // Pull out the key and check it.
    key = tree[lo - 1];
    CHECK(((search ^ key) >> (32 - (key & 0xf))) == 0);

    kit__png_bits(s, key & 0xf);
    return (key >> 4) & 0xfff;
}

static void kit__png_run(kit__PngState* s, int sym) {
    int length = kit__png_bits(s, kit__png_len_bits[sym]) + kit__png_len_base[sym];
    int dsym = kit__png_decode(s, s->distcodes, s->tdist);
    int offs = kit__png_bits(s, kit__png_dist_bits[dsym]) + kit__png_dist_base[dsym];
    kit__png_copy(s, s->out - offs, length);
}

static void kit__png_block(kit__PngState* s) {
    for (;;) {
        int sym = kit__png_decode(s, s->litcodes, s->tlit);
        if (sym < 256) { *kit__png_emit(s, 1) = (unsigned char)sym; } else
        if (sym > 256) { kit__png_run(s, sym - 257); } else { break; }
    }
}

static void kit__png_stored(kit__PngState* s) {
    // Uncompressed data kit__png_block.
    int len;
    kit__png_bits(s, s->count & 7);
    len = kit__png_bits(s, 16);
    CHECK(((len ^ s->bits) & 0xffff) == 0xffff);
    CHECK(s->in + len <= s->inend);

    kit__png_copy(s, s->in, len);
    s->in += len;
    kit__png_bits(s, 16);
}

static void kit__png_fixed(kit__PngState* s) {
    // Fixed set of Huffman codes.
    int n;
    unsigned char lens[288 + 32];
    for (n = 0;   n <= 143; n++) { lens[n] = 8; }
    for (n = 144; n <= 255; n++) { lens[n] = 9; }
    for (n = 256; n <= 279; n++) { lens[n] = 7; }
    for (n = 280; n <= 287; n++) { lens[n] = 8; }
    for (n = 0;   n < 32;   n++) { lens[288 + n] = 5; }

    // Build lit/dist trees.
    s->tlit  = kit__png_build(s, s->litcodes,  lens, 288);
    s->tdist = kit__png_build(s, s->distcodes, lens + 288, 32);
}

static void kit__png_dynamic(kit__PngState* s) {
    int n, i, nlit, ndist, nlen;
    unsigned char lenlens[19] = { 0 }, lens[288 + 32];
    nlit = 257 + kit__png_bits(s, 5);
    ndist = 1 + kit__png_bits(s, 5);
    nlen = 4 + kit__png_bits(s, 4);
    for (n = 0; n < nlen; n++)
        lenlens[(uint8_t) kit__png_order[n]] = (unsigned char)kit__png_bits(s, 3);

    // Build the tree for decoding code lengths.
    s->tlen = kit__png_build(s, s->lencodes, lenlens, 19);

    // Decode code lengths.
    for (n = 0; n < nlit + ndist;) {
        int sym = kit__png_decode(s, s->lencodes, s->tlen);
        switch (sym) {
        case 16: for (i = 3 + kit__png_bits(s, 2); i; i--, n++) { lens[n] = lens[n - 1]; } break;
        case 17: for (i = 3 + kit__png_bits(s, 3); i; i--, n++) { lens[n] = 0; }           break;
        case 18: for (i = 11 + kit__png_bits(s, 7); i; i--, n++) { lens[n] = 0; }          break;
        default: lens[n++] = (unsigned char)sym;                                  break;
        }
    }

    // Build lit/dist trees.
    s->tlit  = kit__png_build(s, s->litcodes,  lens, nlit);
    s->tdist = kit__png_build(s, s->distcodes, lens + nlit, ndist);
}

int kit__png_inflate(void* out, unsigned outlen, const void* in, unsigned inlen) {
    int last;
    kit__PngState state = {0};
    kit__PngState *s = &state;

    // We assume we can buffer 2 extra bytes from off the end of 'in'.
    s->in     = (unsigned char*)in;
    s->inend  = s->in + inlen + 2;
    s->out    = (unsigned char*)out;
    s->outend = s->out + outlen;
    s->bits   = 0;
    s->count  = 0;
    kit__png_bits(s, 0);

    if (setjmp(s->jmp) == 1) { return 0; }

    do {
        last = kit__png_bits(s, 1);
        switch (kit__png_bits(s, 2)) {
        case 0: kit__png_stored(s);            break;
        case 1: kit__png_fixed(s);   kit__png_block(s); break;
        case 2: kit__png_dynamic(s); kit__png_block(s); break;
        case 3: FAIL();
        }
    } while (!last);

    return 1;
}

#undef CHECK
#undef FAIL

static unsigned kit__png_get32(const unsigned char* v) {
    return (v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3];
}

static const unsigned char* kit__png_find(kit__Png* png, const char* chunk, unsigned minlen) {
    const unsigned char* start;
    while (png->p < png->end) {
        unsigned len = kit__png_get32(png->p + 0);
        start = png->p;
        png->p += len + 12;
        if (memcmp(start + 4, chunk, 4) == 0 && len >= minlen && png->p <= png->end)
            return start + 8;
    }

    return NULL;
}

static unsigned char kit__png_paeth(unsigned char a, unsigned char b, unsigned char c) {
    int p = a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}


static int kit__png_row_bytes(int w, int bipp) {
    int rowBits = w * bipp;
    return rowBits / 8 + ((rowBits % 8) ? 1 : 0);
}


static int kit__png_unfilter(int w, int h, int bipp, unsigned char* raw) {
    int len = kit__png_row_bytes(w, bipp);
    int bpp = kit__png_row_bytes(1, bipp);
    int x, y;
    unsigned char* first = (unsigned char*)malloc(len + 1);
    memset(first, 0, len + 1);
    unsigned char* prev = first;
    for (y = 0; y < h; y++, prev = raw, raw += len) {
#define LOOP(A, B)            \
    for (x = 0; x < bpp; x++) \
        raw[x] += A;          \
    for (; x < len; x++)      \
        raw[x] += B;          \
    break
        switch (*raw++) {
        case 0: break;
        case 1: LOOP(0, raw[x - bpp]);
        case 2: LOOP(prev[x], prev[x]);
        case 3: LOOP(prev[x] / 2, (raw[x - bpp] + prev[x]) / 2);
        case 4: LOOP(prev[x], kit__png_paeth(raw[x - bpp], prev[x], prev[x - bpp]));
        default: return 0;
        }
#undef LOOP
    }
    free(first);
    return 1;
}

static void kit__png_convert(int bypp, int w, int h, const unsigned char* src, kit_Color* dest, const unsigned char* trns) {
    int x, y;
    for (y = 0; y < h; y++) {
        src++;  // skip filter byte
        for (x = 0; x < w; x++, src += bypp) {
            switch (bypp) {
            case 1: {
                unsigned char c = src[0];
                if (trns && c == *trns) {
                    *dest++ = kit_rgba(c, c, c, 0);
                    break;
                } else {
                    *dest++ = kit_rgb(c, c, c);
                    break;
                }
            }
            case 2:
                *dest++ = kit_rgba(src[0], src[0], src[0], src[1]);
                break;
            case 3: {
                unsigned char r = src[0];
                unsigned char g = src[1];
                unsigned char b = src[2];
                if (trns && trns[1] == r && trns[3] == g && trns[5] == b) {
                    *dest++ = kit_rgba(r, g, b, 0);
                    break;
                } else {
                    *dest++ = kit_rgb(r, g, b);
                    break;
                }
            }
            case 4:
                *dest++ = kit_rgba(src[0], src[1], src[2], src[3]);
                break;
            }
        }
    }
}

static void kit__png_depalette(int w, int h, unsigned char* src, kit_Color* dest, int bipp, const unsigned char* plte, const unsigned char* trns, int trnsSize) {
    int x, y, c;
    unsigned char alpha;
    int mask = 0;
    int len = 0;

    switch (bipp) {
    case 4: mask = 15; len = 1; break;
    case 2: mask = 3;  len = 3; break;
    case 1: mask = 1;  len = 7; break;
    }

    for (y = 0; y < h; y++) {
        src++;  // skip filter byte
        for (x = 0; x < w; x++) {
            if (bipp == 8) {
                c = *src++;
            } else {
                int pos = x & len;
                c = (src[0] >> ((len - pos) * bipp)) & mask;
                if (pos == len) {
                    src++;
                }
            }
            alpha = 255;
            if (c < trnsSize) {
                alpha = trns[c];
            }
            *dest++ = kit_rgba(plte[c * 3 + 0], plte[c * 3 + 1], plte[c * 3 + 2], alpha);
        }
    }
}

#define FAIL() goto err;
#define CHECK(X) if (!(X)) FAIL()

static int kit__png_outsize(kit_Image* bmp, int bipp) {
    return (kit__png_row_bytes(bmp->w, bipp) + 1) * bmp->h;
}

static kit_Image* kit__load_png(void *png_data, int png_len) {
    kit__Png png = { png_data, ((uint8_t*) png_data) + png_len };

    const unsigned char *ihdr, *idat, *plte, *trns, *first;
    int trnsSize = 0;
    int depth, ctype, bipp;
    int datalen = 0;
    unsigned char *data = NULL, *out;
    kit_Image* bmp = NULL;

    CHECK(memcmp(png.p, "\211PNG\r\n\032\n", 8) == 0);  // kit__Png signature
    png.p += 8;
    first = png.p;

    // Read IHDR
    ihdr = kit__png_find(&png, "IHDR", 13);
    CHECK(ihdr);
    depth = ihdr[8];
    ctype = ihdr[9];
    switch (ctype) {
    case 0: bipp = depth;     break;  // greyscale
    case 2: bipp = depth * 3; break;  // RGB
    case 3: bipp = depth;     break;  // paletted
    case 4: bipp = depth * 2; break;  // grey+alpha
    case 6: bipp = depth * 4; break;  // RGBA
    default: FAIL();
    }

    // Allocate bitmap (+1 width to save room for stupid kit__Png filter bytes)
    bmp = kit_create_image(kit__png_get32(ihdr + 0) + 1, kit__png_get32(ihdr + 4));
    CHECK(bmp);
    bmp->w--;

    // We support 8-bit color components and 1, 2, 4 and 8 bit palette formats.
    // No interlacing, or wacky filter types.
    CHECK((depth != 16) && ihdr[10] == 0 && ihdr[11] == 0 && ihdr[12] == 0);

    // Join IDAT chunks.
    for (idat = kit__png_find(&png, "IDAT", 0); idat; idat = kit__png_find(&png, "IDAT", 0)) {
        unsigned len = kit__png_get32(idat - 8);
        data = (unsigned char*)realloc(data, datalen + len);
        if (!data)
            break;

        memcpy(data + datalen, idat, len);
        datalen += len;
    }

    // Find palette.
    png.p = first;
    plte = kit__png_find(&png, "PLTE", 0);

    // Find transparency info.
    png.p = first;
    trns = kit__png_find(&png, "tRNS", 0);
    if (trns) { trnsSize = kit__png_get32(trns - 8); }

    CHECK(data && datalen >= 6);
    CHECK(
           (data[0] & 0x0f) == 0x08  // compression method (RFC 1950)
        && (data[0] & 0xf0) <= 0x70  // window size
        && (data[1] & 0x20) == 0     // preset dictionary present
    );

    out = (unsigned char*)bmp->pixels + kit__png_outsize(bmp, 32) - kit__png_outsize(bmp, bipp);
    CHECK(kit__png_inflate(out, kit__png_outsize(bmp, bipp), data + 2, datalen - 6));
    CHECK(kit__png_unfilter(bmp->w, bmp->h, bipp, out));

    if (ctype == 3) {
        CHECK(plte);
        kit__png_depalette(bmp->w, bmp->h, out, bmp->pixels, bipp, plte, trns, trnsSize);
    } else {
        CHECK(bipp % 8 == 0);
        kit__png_convert(bipp / 8, bmp->w, bmp->h, out, bmp->pixels, trns);
    }

    free(data);
    return bmp;

err:
    if (data) { free(data); }
    if (bmp)  { free(bmp);  }
    return NULL;
}

#undef CHECK
#undef FAIL


//////////////////////////////////////////////////////////////////////////////
// Embedded font
//////////////////////////////////////////////////////////////////////////////

static char kit__font_png[] = {
0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x01, 0x00,
0x01, 0x03, 0x00, 0x00, 0x00, 0xdf, 0x8d, 0xeb, 0x44, 0x00, 0x00, 0x00,
0x06, 0x50, 0x4c, 0x54, 0x45, 0x47, 0x70, 0x4c, 0xff, 0xff, 0xff, 0x9f,
0x94, 0xa2, 0x43, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00,
0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x05, 0x4d, 0x49, 0x44, 0x41, 0x54,
0x58, 0xc3, 0xed, 0x98, 0x3f, 0x88, 0x24, 0x45, 0x14, 0xc6, 0xdf, 0x74,
0x37, 0xdd, 0xd5, 0xbb, 0x75, 0xd3, 0x13, 0x4e, 0x70, 0x48, 0x7b, 0x98,
0x08, 0x06, 0x03, 0x06, 0x2e, 0xb8, 0x4a, 0xbb, 0xd1, 0x04, 0x07, 0x5e,
0x62, 0xde, 0x82, 0x5c, 0x28, 0x03, 0x82, 0x0c, 0xc7, 0xa1, 0x4f, 0x5c,
0xb8, 0x0b, 0x56, 0x9c, 0xf0, 0x44, 0x11, 0x03, 0x95, 0xc5, 0x4c, 0x30,
0x50, 0x30, 0xa8, 0x03, 0x83, 0xe3, 0x18, 0xf5, 0xc2, 0x0d, 0xd7, 0xdc,
0x60, 0xc2, 0x09, 0x46, 0xcf, 0xef, 0x55, 0x55, 0x77, 0x4f, 0xcf, 0x1f,
0x6f, 0x11, 0x5d, 0xf7, 0x8e, 0x2b, 0x8a, 0xde, 0x9e, 0xfe, 0xf5, 0xab,
0x7a, 0xaf, 0x5e, 0x7d, 0xd5, 0x55, 0x4b, 0xb4, 0xb5, 0x24, 0x3c, 0xae,
0xab, 0xfb, 0x49, 0x84, 0x7b, 0xdc, 0xb2, 0x6a, 0xaa, 0xfb, 0x49, 0xb8,
0x7f, 0x72, 0xc0, 0xf6, 0xc8, 0xff, 0xc7, 0x32, 0x5e, 0xf6, 0x6a, 0xa9,
0x92, 0x6a, 0xc5, 0xd1, 0xd4, 0x27, 0x06, 0x6c, 0x8d, 0xfc, 0xe2, 0x96,
0x4e, 0x9e, 0xc7, 0xa7, 0xf8, 0x9b, 0xab, 0xfa, 0x11, 0xdc, 0xd5, 0x34,
0xe9, 0xe4, 0xa3, 0x34, 0x1f, 0x2b, 0x2a, 0x22, 0xca, 0x5b, 0x20, 0x99,
0x1d, 0xa7, 0xc5, 0x54, 0x91, 0x89, 0xe9, 0xd8, 0xb7, 0x41, 0x16, 0x04,
0xc5, 0x69, 0x5c, 0xfc, 0x6c, 0xc1, 0x28, 0x58, 0x02, 0x04, 0x40, 0xfc,
0x26, 0x59, 0x0b, 0x07, 0x02, 0x07, 0xb2, 0xd3, 0x11, 0xf1, 0x2f, 0x16,
0xe4, 0x19, 0x13, 0xaa, 0x07, 0x11, 0xef, 0x75, 0x06, 0x53, 0x0b, 0xc8,
0x59, 0x44, 0xae, 0x2b, 0x01, 0xc3, 0x35, 0xa0, 0xe5, 0xe6, 0x38, 0xa8,
0x00, 0xae, 0xa8, 0xaa, 0x02, 0xa3, 0x60, 0x00, 0x59, 0x14, 0xd1, 0x32,
0xb0, 0xc5, 0xc5, 0xe5, 0x22, 0x2f, 0xb6, 0x0d, 0xd0, 0x45, 0x48, 0xde,
0x38, 0xa6, 0x71, 0x66, 0x74, 0x66, 0xf6, 0x32, 0xd3, 0x4c, 0x27, 0xdc,
0x4c, 0xbb, 0x74, 0xd4, 0xe1, 0xcb, 0x9a, 0x4a, 0x32, 0x53, 0x8d, 0xe0,
0xa8, 0x2f, 0x01, 0x1a, 0x9a, 0xc6, 0xa4, 0x43, 0x7a, 0x41, 0x23, 0x60,
0x0f, 0x06, 0x24, 0x2f, 0x59, 0x90, 0xf0, 0xd5, 0x8c, 0xe7, 0x64, 0xc6,
0x16, 0x94, 0x99, 0x19, 0x74, 0x58, 0x40, 0x9f, 0xcc, 0x11, 0x5e, 0xef,
0x30, 0x2c, 0x8c, 0x98, 0x52, 0x3f, 0x24, 0x01, 0x03, 0x32, 0x7f, 0x02,
0x84, 0x34, 0x4d, 0x04, 0x88, 0x85, 0x03, 0x68, 0x51, 0x3b, 0x40, 0x75,
0x1f, 0x16, 0xc0, 0x07, 0xad, 0x2c, 0xe8, 0x30, 0x5e, 0x11, 0xaf, 0x42,
0x1b, 0xc7, 0x22, 0x61, 0xf4, 0x3f, 0x0e, 0xa1, 0x15, 0xb1, 0x95, 0x12,
0xae, 0x07, 0x1c, 0x6d, 0x1b, 0x09, 0x75, 0x31, 0x54, 0x93, 0xf0, 0x1c,
0x35, 0x33, 0x88, 0x66, 0xaa, 0x49, 0xeb, 0xd2, 0x28, 0xfd, 0x01, 0xf2,
0xf1, 0xaa, 0xc6, 0x68, 0x17, 0x78, 0x64, 0xec, 0x55, 0xcb, 0x38, 0x5e,
0xbe, 0x8b, 0x61, 0x7e, 0x4d, 0x00, 0x59, 0x20, 0x57, 0x7d, 0x89, 0x4d,
0x77, 0xff, 0x37, 0x80, 0xcf, 0x8e, 0xda, 0x20, 0x23, 0x93, 0xdd, 0xfc,
0x16, 0xe0, 0xcb, 0x8f, 0xcd, 0x1c, 0x8f, 0x90, 0xa8, 0x4b, 0x66, 0x01,
0xd0, 0x25, 0xb3, 0xfb, 0xfb, 0x27, 0x00, 0x5f, 0x37, 0x16, 0xf6, 0x06,
0x16, 0xe9, 0xf4, 0x23, 0x80, 0x4f, 0x75, 0x1b, 0xa0, 0x0f, 0x75, 0x74,
0x0b, 0xa0, 0x68, 0xbc, 0x72, 0x6f, 0x18, 0xb8, 0xfb, 0x21, 0x00, 0xdc,
0x75, 0x71, 0x18, 0x1b, 0xc7, 0x58, 0x97, 0x73, 0x17, 0xc7, 0xe3, 0x51,
0xba, 0xc4, 0x5d, 0x2b, 0x62, 0x09, 0x22, 0xa1, 0x85, 0x44, 0x37, 0x45,
0x74, 0x18, 0x2e, 0x8e, 0xe9, 0x9a, 0xcc, 0x44, 0x79, 0xc4, 0x83, 0x0a,
0x68, 0x4d, 0x45, 0x4c, 0xb7, 0x2b, 0x40, 0x2b, 0x80, 0x3c, 0xe8, 0x36,
0xa0, 0xaf, 0x29, 0x77, 0x60, 0x2e, 0x63, 0xe5, 0x01, 0x34, 0x38, 0xa8,
0x81, 0x0c, 0x6a, 0x58, 0xf5, 0x11, 0x52, 0xa9, 0xa9, 0xd7, 0x80, 0x4e,
0x03, 0x4c, 0x0b, 0xa4, 0x02, 0xee, 0xe9, 0x7b, 0x0e, 0x28, 0x0f, 0x90,
0x06, 0xf4, 0x21, 0x29, 0x99, 0x01, 0x2c, 0x1c, 0xc8, 0x66, 0x6e, 0xe1,
0x6a, 0x45, 0x2e, 0xf5, 0xc2, 0x16, 0xb6, 0xfe, 0x42, 0xdd, 0x7b, 0x12,
0x2c, 0xf5, 0x75, 0xe5, 0x7e, 0x51, 0x81, 0xd2, 0x45, 0x54, 0x83, 0xdc,
0x66, 0xf0, 0x66, 0xc2, 0xb3, 0x44, 0xa6, 0x7d, 0x1f, 0x53, 0x34, 0xfb,
0x7e, 0x2e, 0x33, 0x51, 0x49, 0x06, 0x91, 0x89, 0xd2, 0x66, 0xac, 0x8f,
0x49, 0x6d, 0x13, 0x03, 0x0d, 0xfa, 0xb9, 0xec, 0x01, 0x64, 0xe0, 0x81,
0x97, 0x45, 0x05, 0xf4, 0x0a, 0xc0, 0xfa, 0xb5, 0xc9, 0x02, 0x7d, 0x14,
0x1b, 0xfa, 0x70, 0x5e, 0x95, 0x89, 0x6b, 0xca, 0x08, 0x68, 0xeb, 0x43,
0x02, 0xb8, 0xb3, 0x69, 0xa5, 0x12, 0x30, 0xda, 0xb8, 0x76, 0xfc, 0x47,
0x4b, 0xd2, 0xda, 0xa3, 0xc0, 0x6d, 0x17, 0x2b, 0x50, 0x2e, 0x2d, 0xaf,
0x1c, 0xfd, 0x3d, 0x98, 0x27, 0xe6, 0x24, 0xa1, 0x59, 0xad, 0x28, 0x0b,
0xde, 0x02, 0xc0, 0x8f, 0xb9, 0xe6, 0xb2, 0x02, 0xca, 0x82, 0x3b, 0x0e,
0x98, 0x2e, 0xd5, 0xa0, 0x87, 0xfd, 0x50, 0x60, 0x9b, 0x12, 0x90, 0x78,
0x80, 0x39, 0x9f, 0xd7, 0x7d, 0x08, 0x08, 0x9b, 0xa6, 0x8a, 0x16, 0x48,
0x05, 0x88, 0x3e, 0x70, 0xbf, 0xec, 0x95, 0x11, 0x7d, 0x98, 0x19, 0xf4,
0x91, 0x78, 0xaf, 0x24, 0x72, 0xd3, 0xd2, 0x87, 0xa9, 0x22, 0xa7, 0x36,
0x50, 0xe6, 0xec, 0x63, 0xf7, 0x74, 0xbf, 0xfb, 0xd8, 0xee, 0x77, 0x9f,
0xe6, 0xe3, 0xc2, 0x9f, 0x3f, 0xe0, 0x92, 0xf3, 0xca, 0x1f, 0x2d, 0x12,
0x3a, 0x21, 0x7a, 0x78, 0x4a, 0x0b, 0x59, 0x93, 0xec, 0x31, 0x63, 0x14,
0x32, 0xe9, 0x01, 0x2b, 0x3e, 0x09, 0x0d, 0xcb, 0x5b, 0xbc, 0xef, 0xc0,
0x71, 0x5c, 0x50, 0x5c, 0xb2, 0x1c, 0x12, 0x72, 0x00, 0x58, 0x1c, 0xb8,
0x85, 0xec, 0x8b, 0x98, 0x0e, 0xb3, 0x19, 0x3e, 0xfc, 0x94, 0x4e, 0xf8,
0x59, 0x98, 0xf0, 0x0f, 0xce, 0x02, 0x00, 0xdf, 0x61, 0xde, 0x25, 0xda,
0xb9, 0xc6, 0x57, 0x0a, 0x58, 0x7c, 0xe5, 0x9b, 0xca, 0x68, 0x81, 0x8f,
0x6b, 0xca, 0xb4, 0xd3, 0xe3, 0xe7, 0xb8, 0xb1, 0x18, 0x61, 0x9f, 0x0a,
0x0b, 0xcd, 0x62, 0xf1, 0x3c, 0x35, 0x16, 0xf8, 0xe4, 0x7f, 0x07, 0x10,
0xb3, 0xf4, 0x61, 0x2d, 0x3c, 0xa0, 0x38, 0xff, 0x03, 0x20, 0xb4, 0x5e,
0xd9, 0x3e, 0x0e, 0x3c, 0xc8, 0xcc, 0x8d, 0xbb, 0xcc, 0xd8, 0x60, 0x23,
0x0e, 0x78, 0x95, 0xf0, 0x7e, 0x2b, 0x1f, 0x4a, 0x7e, 0xd8, 0xc8, 0x57,
0x87, 0xe8, 0x7c, 0xd2, 0x50, 0x2b, 0xa6, 0xf5, 0x1d, 0x20, 0xfe, 0xe6,
0xf8, 0xb6, 0xdd, 0x6e, 0xdb, 0x75, 0x74, 0x12, 0xd1, 0x50, 0x4b, 0x8a,
0xde, 0x23, 0x7e, 0xe3, 0xe0, 0x4a, 0xe9, 0x00, 0x82, 0x9d, 0x28, 0x9a,
0x58, 0x70, 0xdf, 0x5a, 0x4c, 0x6a, 0x80, 0x5d, 0xd0, 0x09, 0x8c, 0xe2,
0xf2, 0xbe, 0xb5, 0x98, 0xa4, 0x55, 0x53, 0x18, 0x92, 0x13, 0x18, 0x59,
0x00, 0x8b, 0x32, 0x62, 0x6f, 0xe1, 0x4b, 0x5c, 0xde, 0xb0, 0x16, 0x38,
0x34, 0xae, 0x82, 0x1d, 0xe7, 0x55, 0x27, 0x17, 0x10, 0xb7, 0x41, 0x63,
0xa1, 0xd7, 0x2d, 0x5c, 0x1f, 0xba, 0x3e, 0xe2, 0x25, 0x0f, 0x76, 0x12,
0x6b, 0x91, 0x52, 0x5b, 0x1f, 0xf6, 0xd8, 0xe1, 0x22, 0xff, 0xf7, 0x8f,
0x1d, 0x45, 0x87, 0x06, 0x71, 0x31, 0x0c, 0xdd, 0x3e, 0x88, 0x07, 0x11,
0xe3, 0xe3, 0x25, 0x43, 0x9e, 0x07, 0x34, 0x4c, 0x79, 0x18, 0x89, 0x1b,
0x79, 0x87, 0x70, 0x83, 0x40, 0x30, 0x62, 0xd4, 0x8b, 0xe8, 0xd0, 0x9e,
0x79, 0x71, 0xe9, 0x05, 0x9b, 0x56, 0xb8, 0xf7, 0x6f, 0xf1, 0x02, 0xa7,
0x90, 0xcc, 0x9e, 0xca, 0x62, 0xbf, 0xad, 0x75, 0xb5, 0x30, 0xf6, 0x4c,
0xb1, 0x0e, 0xe8, 0x6c, 0xc0, 0xb5, 0x8b, 0xfa, 0x50, 0xd3, 0x1c, 0xa7,
0x90, 0x8c, 0xff, 0x71, 0x53, 0x8f, 0xf6, 0xea, 0xd7, 0x95, 0x38, 0x9a,
0x12, 0x6e, 0xfd, 0xc7, 0xc5, 0x36, 0x10, 0x9c, 0x3d, 0x25, 0xb9, 0x4d,
0x89, 0x33, 0xea, 0xd5, 0x3d, 0x8d, 0xc3, 0xbc, 0xe7, 0x52, 0x62, 0xbb,
0xc9, 0x23, 0x1e, 0x3a, 0x7d, 0xa8, 0x68, 0x7d, 0x03, 0x61, 0x27, 0xb5,
0x52, 0xbd, 0xfa, 0x9f, 0x70, 0xcf, 0x1c, 0xd9, 0x98, 0x64, 0xee, 0x1a,
0x52, 0xba, 0xe7, 0x63, 0x24, 0x7a, 0x51, 0x57, 0x20, 0x2e, 0x49, 0x75,
0x2b, 0x10, 0x15, 0x6d, 0xb0, 0x5b, 0x81, 0x80, 0x5f, 0x6a, 0x00, 0x9a,
0x4a, 0x27, 0x1e, 0x74, 0xe8, 0xe5, 0x96, 0x85, 0x7a, 0x50, 0x5b, 0xb4,
0x9b, 0x52, 0xa3, 0x2d, 0x7d, 0xa8, 0x41, 0xed, 0xd5, 0x3b, 0xcb, 0x60,
0xbc, 0x14, 0xc7, 0xe7, 0xf5, 0xf7, 0xa3, 0x6b, 0xce, 0xf3, 0xd3, 0xb0,
0x9e, 0xf0, 0x1c, 0x62, 0xa9, 0x64, 0x20, 0x22, 0xaa, 0x41, 0x0f, 0x62,
0x81, 0xa2, 0x9c, 0x70, 0x02, 0x11, 0x8e, 0x28, 0x0a, 0xc2, 0x51, 0x01,
0x1d, 0x22, 0x7d, 0xb6, 0x29, 0x11, 0x51, 0xad, 0xa8, 0x47, 0xef, 0x19,
0xa0, 0x28, 0x77, 0xe3, 0x67, 0xe2, 0x91, 0x32, 0xbe, 0x4e, 0x0b, 0x37,
0x68, 0x1e, 0xdc, 0x4c, 0x8c, 0xaf, 0x53, 0x6a, 0x81, 0x7a, 0x52, 0xbf,
0xae, 0x69, 0x9b, 0x38, 0xb7, 0xe9, 0xe3, 0x6a, 0xf1, 0x93, 0x96, 0xba,
0xda, 0xc7, 0xaa, 0x57, 0xe7, 0xa4, 0x8f, 0x2d, 0x63, 0xb5, 0x9c, 0x86,
0xea, 0x5f, 0xce, 0x76, 0x7f, 0x15, 0x17, 0x92, 0x06, 0xa4, 0xc4, 0x29,
0x23, 0xa6, 0x6a, 0xc3, 0x92, 0xb2, 0xa4, 0xc1, 0x29, 0x0a, 0x8f, 0xd2,
0x1a, 0xac, 0x69, 0xa9, 0x02, 0x59, 0x35, 0xfb, 0x02, 0x7a, 0xa5, 0xd5,
0x54, 0x3d, 0x56, 0x44, 0xd7, 0x37, 0x83, 0x8c, 0xaf, 0x6f, 0xb3, 0x78,
0x7b, 0x33, 0x08, 0xe8, 0xdd, 0x33, 0xf5, 0x31, 0xd6, 0xa6, 0xd6, 0xc7,
0x8f, 0xb8, 0xc6, 0x74, 0x7e, 0xfb, 0xab, 0xbf, 0x00, 0xec, 0x51, 0xe8,
0xd2, 0x17, 0x57, 0x55, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
0x44, 0xae, 0x42, 0x60, 0x82,
};

static void *kit__font_png_data = kit__font_png;
static int   kit__font_png_size = sizeof(kit__font_png);

#endif // KIT_IMPL