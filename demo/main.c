#define KIT_IMPL
#include "../kit.h"
#include "assets.h"

typedef struct {
    float x, y;
    float vx, vy;
    float frame;
} Entity;

int main(void) {
    // initialize everything
    kit_Context *ctx = kit_create("Demo", 100, 100, KIT_SCALE3X | KIT_HIDECURSOR);
    kit_Image *corner = kit_load_image_mem(corner_png, sizeof(corner_png));
    kit_Image *cursor = kit_load_image_mem(cursor_png, sizeof(cursor_png));
    kit_Image *sprite = kit_load_image_mem(sprite_png, sizeof(sprite_png));
    Entity e = { .x = ctx->screen->w / 2, .y = ctx->screen->h / 2 };

    double dt;
    while (kit_step(ctx, &dt)) {
        // handle keyboard input
        if (kit_key_pressed(ctx, VK_ESCAPE)) { break; }

        // get mouse position
        int mx, my;
        kit_mouse_pos(ctx, &mx, &my);

        // update entity
        if (e.x + 5 < mx) { e.vx += 3.0; } else
        if (e.x - 5 > mx) { e.vx -= 3.0; } else
                          { e.vx *= 0.9; }
        if (e.y + 5 < my) { e.vy += 3.0; } else
        if (e.y - 5 > my) { e.vy -= 3.0; } else
                          { e.vy *= 0.9; }
        if (fabs(e.vx) > 30) { e.vx = 30 * (e.vx < 0 ? -1 : 1); }
        if (fabs(e.vy) > 30) { e.vy = 30 * (e.vy < 0 ? -1 : 1); }
        if (mx | my) {
            e.x += e.vx * dt;
            e.y += e.vy * dt;
        }
        e.frame += dt / 0.1;

        // clear screen
        kit_clear(ctx, kit_rgb(0xff, 0xff, 0xff));

        // get animation frame and draw entity
        int frame = ((int) e.frame) % 4;
        if (fabs(e.x - mx) < 7 && fabs(e.y - my) < 7) {
            frame += 8;
        } else if (mx | my) {
            frame += 4;
        }
        kit_Rect r = { frame * 9, 0, 9, 19 };
        kit_draw_image2(ctx, KIT_WHITE, sprite, e.x - 4, e.y - 17, r);

        // draw mouse cursor sprite
        if (mx | my) {
            kit_draw_image(ctx, cursor, mx, my);
        }

        // draw rounded corners
        kit_draw_image2(ctx, KIT_WHITE, corner, 0, 0, kit_rect(0, 0, 6, 6));
        kit_draw_image2(ctx, KIT_WHITE, corner, ctx->screen->w-6, 0, kit_rect(5, 0, -6, 6));
        kit_draw_image2(ctx, KIT_WHITE, corner, 0, ctx->screen->h-6, kit_rect(0, 5, 6, -6));
        kit_draw_image2(ctx, KIT_WHITE, corner, ctx->screen->w-6, ctx->screen->h-6, kit_rect(5, 5, -8, -8));
    }

    // clean up everything
    kit_destroy_image(sprite);
    kit_destroy_image(cursor);
    kit_destroy_image(corner);
    kit_destroy(ctx);

    return 0;
}