/*
 * Hoptics PC simulator
 *
 *   ./hoptics_sim                 open a 1024x600 window (needs SDL2)
 *   ./hoptics_sim --shots <dir>   render screenshots without a window
 *
 * SPDX-License-Identifier: MIT
 */
#include "lvgl.h"
#include "bs_ui.h"
#include "bs_model.h"
#include "fake_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BS_SIM_SDL
#include <SDL2/SDL.h>
#endif

/* ---------- headless mode ---------- */
static uint32_t s_sim_ms;
static uint32_t *s_fb;

static uint32_t sim_tick(void) { return s_sim_ms; }

static void fb_flush(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    LV_UNUSED(area);
    memcpy(s_fb, px, BS_SCREEN_W * BS_SCREEN_H * 4);
    lv_display_flush_ready(d);
}

static void run_for(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 33) {
        s_sim_ms += 33;
        fake_mesh_step(s_sim_ms);
        lv_timer_handler();
    }
}

static int save_ppm(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    fprintf(f, "P6\n%d %d\n255\n", BS_SCREEN_W, BS_SCREEN_H);
    for (int i = 0; i < BS_SCREEN_W * BS_SCREEN_H; i++) {
        uint32_t p = s_fb[i];  /* XRGB8888 */
        unsigned char rgb[3] = { (p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("wrote %s\n", path);
    return 0;
}

static int headless(const char *dir)
{
    lv_tick_set_cb(sim_tick);
    size_t bytes = BS_SCREEN_W * BS_SCREEN_H * 4;
    s_fb = malloc(bytes);
    void *buf = malloc(bytes);
    if (!s_fb || !buf) return 1;

    lv_display_t *d = lv_display_create(BS_SCREEN_W, BS_SCREEN_H);
    lv_display_set_flush_cb(d, fb_flush);
    lv_display_set_buffers(d, buf, NULL, bytes, LV_DISPLAY_RENDER_MODE_FULL);

    bs_model_init();
    fake_mesh_init();
    bs_ui_create();

    run_for(9000);
    save_ppm(dir, "01-map");

    bs_ui_select_repeater(3);   /* a repeater with a demo Advisor suggestion */
    run_for(1200);
    save_ppm(dir, "02-repeater-detail");

    bs_ui_select_repeater(-1);
    bs_ui_show_sonar(true);
    run_for(4200);
    save_ppm(dir, "03-sonar");
    return 0;
}

/* ---------- window mode ---------- */
#ifdef BS_SIM_SDL
static int windowed(void)
{
    lv_display_t *d = lv_sdl_window_create(BS_SCREEN_W, BS_SCREEN_H);
    lv_display_set_default(d);
    lv_sdl_mouse_create();
    lv_tick_set_cb(SDL_GetTicks);

    bs_model_init();
    fake_mesh_init();
    bs_ui_create();

    for (;;) {
        fake_mesh_step(lv_tick_get());
        uint32_t wait = lv_timer_handler();
        SDL_Delay(wait < 5 ? wait : 5);
    }
    return 0;
}
#endif

int main(int argc, char **argv)
{
    lv_init();
    if (argc == 3 && strcmp(argv[1], "--shots") == 0)
        return headless(argv[2]);
#ifdef BS_SIM_SDL
    return windowed();
#else
    fprintf(stderr, "Built without SDL2. Use: %s --shots <dir>\n", argv[0]);
    return 1;
#endif
}
