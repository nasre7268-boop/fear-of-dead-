/* Host-side sanity test: runs the game headless and dumps PPM screenshots. */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"

static void dump(const char *name, uint32_t *fb) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) { unsigned c = fb[i]; fputc(c >> 16, f); fputc(c >> 8, f); fputc(c, f); }
    fclose(f);
}
int main(void) {
    uint32_t *fb = malloc(SCR_W * SCR_H * 4);
    Input in = {0};
    game_init();
    for (int i = 0; i < 40; i++) game_update(&in);
    game_render(fb); dump("shot_title.ppm", fb);
    in.start = 1; game_update(&in); in.start = 0;
    for (int f = 0; f < 60 * 400; f++) {                 /* bot: spin & slash */
        in.lx = (f % 240 < 120) ? 0.7f : -0.7f; in.ly = ((f / 7) % 3 - 1) * 0.5f;
        in.attack = (f % 20) < 2; in.dodge = (f % 150) == 0;
        game_update(&in);
        if (f == 600) { game_render(fb); dump("shot_play.ppm", fb); }
    }
    game_render(fb); dump("shot_end.ppm", fb);
    puts("headless run OK");
    return 0;
}
