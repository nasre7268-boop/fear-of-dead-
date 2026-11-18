/* FEAR OF DEAD - Trial Version
 * Original action-survival game. Platform independent: draws into a
 * 1280x720 XRGB buffer. PS3 glue lives in main_ps3.c.
 */
#include <math.h>
#include <string.h>
#include "game.h"

#define DT          (1.0f/60.0f)
#define MAX_ENEMY   40
#define TRIAL_WAVES 3

enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_TRIAL_END };
enum { T_WRAITH, T_BRUTE, T_KING };

typedef struct {
    int   on, type;
    float x, y, kx, ky, hp, maxhp, r, speed, dmg;
    float flash, hit_cd;
    int   swing_id;
} Enemy;

typedef struct {
    float x, y, fx, fy, hp, atk_t, atk_cd, roll_t, roll_cd, inv, rx, ry;
    int   swing;
} Player;

static int    state, wave, to_spawn, prev_attack, prev_dodge, prev_start;
static float  spawn_t, wave_banner, shake, state_t;
static int    score, kills;
static Player P;
static Enemy  E[MAX_ENEMY];
static uint32_t rng = 12345u;
static unsigned frame_no;

static float frand(void) {
    rng = rng * 1664525u + 1013904223u;
    return (float)((rng >> 8) & 0xFFFF) / 65535.0f;
}

/* ------------------------------------------------------------- font */
static const char FONT_CH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-!./";
static const uint8_t FONT[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
 {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
 {0,4,0,0,4,0,0},{0,0,0,31,0,0,0},{4,4,4,4,4,0,4},{0,0,0,0,0,0,4},{1,1,2,4,8,16,16}
};

_Static_assert(sizeof(FONT)/sizeof(FONT[0]) == sizeof(FONT_CH)-1, "font table mismatch");

/* ------------------------------------------------------- primitives */
static uint32_t *FB;

static void px(int x, int y, uint32_t c) {
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) FB[y * SCR_W + x] = c;
}
static void rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c);
}
static void circle(float cx, float cy, float r, uint32_t c) {
    int x0 = (int)(cx - r), x1 = (int)(cx + r), y0 = (int)(cy - r), y1 = (int)(cy + r);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy <= r * r) px(x, y, c);
    }
}
static void sector(float cx, float cy, float fx, float fy, float r0, float r1, float cosmin, uint32_t c) {
    int x0 = (int)(cx - r1), x1 = (int)(cx + r1), y0 = (int)(cy - r1), y1 = (int)(cy + r1);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
        if (d2 < r0 * r0 || d2 > r1 * r1) continue;
        float d = sqrtf(d2);
        if ((dx * fx + dy * fy) / d >= cosmin) px(x, y, c);
    }
}
static void text(int x, int y, int s, uint32_t c, const char *str) {
    for (; *str; str++, x += 6 * s) {
        const char *p = strchr(FONT_CH, *str);
        if (*str == ' ' || !p) continue;
        const uint8_t *g = FONT[p - FONT_CH];
        for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++)
            if (g[r] & (16 >> b)) rect(x + b * s, y + r * s, s, s, c);
    }
}
static int text_w(int s, const char *str) { return (int)strlen(str) * 6 * s; }
static void text_c(int y, int s, uint32_t c, const char *str) {
    text((SCR_W - text_w(s, str)) / 2, y, s, c, str);
}
static void itoa_s(int v, char *o) {
    char t[12]; int n = 0;
    if (v <= 0) { o[0] = '0'; o[1] = 0; return; }
    while (v) { t[n++] = '0' + v % 10; v /= 10; }
    for (int i = 0; i < n; i++) o[i] = t[n - 1 - i];
    o[n] = 0;
}

/* ------------------------------------------------------------ logic */
static void spawn_enemy(int type) {
    for (int i = 0; i < MAX_ENEMY; i++) if (!E[i].on) {
        Enemy *e = &E[i]; memset(e, 0, sizeof *e);
        int side = (int)(frand() * 4.0f) & 3;
        e->x = side == 0 ? 70 : side == 1 ? SCR_W - 70 : 120 + frand() * (SCR_W - 240);
        e->y = side == 2 ? 130 : side == 3 ? SCR_H - 50 : 140 + frand() * (SCR_H - 200);
        e->on = 1; e->type = type; e->swing_id = -1;
        if (type == T_WRAITH) { e->hp = e->maxhp = 40;  e->r = 20; e->speed = 1.5f; e->dmg = 8; }
        if (type == T_BRUTE)  { e->hp = e->maxhp = 120; e->r = 32; e->speed = 1.0f; e->dmg = 16; }
        if (type == T_KING)   { e->hp = e->maxhp = 600; e->r = 56; e->speed = 0.9f; e->dmg = 28; }
        return;
    }
}

static void start_wave(int w) {
    wave = w; wave_banner = 2.5f; spawn_t = 1.0f;
    to_spawn = 3 + w * 3;
}

static void new_game(void) {
    memset(E, 0, sizeof E);
    memset(&P, 0, sizeof P);
    P.x = SCR_W / 2; P.y = SCR_H / 2; P.fx = 1; P.fy = 0; P.hp = 100;
    score = kills = 0; shake = 0;
    start_wave(1);
    state = ST_PLAY; state_t = 0;
}

void game_init(void) {
    state = ST_TITLE; state_t = 0; frame_no = 0;
    prev_attack = prev_dodge = prev_start = 0;
}

static int alive_enemies(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMY; i++) n += E[i].on;
    return n;
}

static void update_play(const Input *in, int atk_press, int dodge_press) {
    float mx = in->lx, my = in->ly, ml = sqrtf(mx * mx + my * my);
    if (ml < 0.2f) { mx = my = 0; ml = 0; } else if (ml > 1) { mx /= ml; my /= ml; ml = 1; }

    if (P.atk_cd > 0) P.atk_cd -= DT;
    if (P.atk_t > 0)  P.atk_t  -= DT;
    if (P.roll_cd > 0) P.roll_cd -= DT;
    if (P.inv > 0) P.inv -= DT;
    if (shake > 0) shake -= DT * 20.0f;

    if (P.roll_t > 0) {                       /* dodge roll */
        P.roll_t -= DT;
        P.x += P.rx * 9.0f; P.y += P.ry * 9.0f;
    } else {
        if (ml > 0) {
            P.x += mx * 4.2f; P.y += my * 4.2f;
            if (P.atk_t <= 0) { P.fx = mx / ml; P.fy = my / ml; }
        }
        if (dodge_press && P.roll_cd <= 0) {
            P.roll_t = 0.28f; P.roll_cd = 0.8f; P.inv = 0.35f;
            P.rx = ml > 0 ? mx / ml : P.fx; P.ry = ml > 0 ? my / ml : P.fy;
        }
        if (atk_press && P.atk_cd <= 0) {
            P.atk_t = 0.22f; P.atk_cd = 0.38f; P.swing++;
        }
    }
    if (P.x < 60) P.x = 60;
    if (P.x > SCR_W - 60) P.x = SCR_W - 60;
    if (P.y < 120) P.y = 120;
    if (P.y > SCR_H - 40) P.y = SCR_H - 40;

    /* spawning / waves */
    if (to_spawn > 0) {
        spawn_t -= DT;
        if (spawn_t <= 0) {
            int t = T_WRAITH;
            if (wave >= 2 && frand() < 0.25f) t = T_BRUTE;
            if (wave == TRIAL_WAVES && to_spawn == 1) t = T_KING;
            spawn_enemy(t); to_spawn--; spawn_t = 0.8f;
        }
    } else if (alive_enemies() == 0) {
        if (wave >= TRIAL_WAVES) { state = ST_TRIAL_END; state_t = 0; return; }
        start_wave(wave + 1);
        P.hp += 25; if (P.hp > 100) P.hp = 100;
    }
    if (wave_banner > 0) wave_banner -= DT;

    /* enemies */
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &E[i]; if (!e->on) continue;
        float dx = P.x - e->x, dy = P.y - e->y, d = sqrtf(dx * dx + dy * dy) + 0.001f;
        e->x += dx / d * e->speed + e->kx; e->y += dy / d * e->speed + e->ky;
        e->kx *= 0.85f; e->ky *= 0.85f;
        if (e->flash > 0) e->flash -= DT;
        if (e->hit_cd > 0) e->hit_cd -= DT;
        for (int j = i + 1; j < MAX_ENEMY; j++) {          /* soft separation */
            Enemy *o = &E[j]; if (!o->on) continue;
            float ax = o->x - e->x, ay = o->y - e->y, ad = sqrtf(ax * ax + ay * ay) + 0.001f;
            float min = e->r + o->r;
            if (ad < min) { float p = (min - ad) * 0.5f; e->x -= ax / ad * p; e->y -= ay / ad * p; o->x += ax / ad * p; o->y += ay / ad * p; }
        }
        if (d < e->r + 18 && e->hit_cd <= 0 && P.inv <= 0) {
            P.hp -= e->dmg; e->hit_cd = 0.9f; P.inv = 0.5f; shake = 6;
            if (P.hp <= 0) { P.hp = 0; state = ST_DEAD; state_t = 0; return; }
        }
        /* player axe hit */
        if (P.atk_t > 0 && e->swing_id != P.swing) {
            float hx = e->x - P.x, hy = e->y - P.y, hd = sqrtf(hx * hx + hy * hy) + 0.001f;
            if (hd < 125 + e->r && (hx * P.fx + hy * P.fy) / hd > 0.25f) {
                e->swing_id = P.swing; e->hp -= 20; e->flash = 0.12f;
                e->kx = hx / hd * 9; e->ky = hy / hd * 9; shake = 3;
                if (e->hp <= 0) {
                    e->on = 0; kills++;
                    score += e->type == T_KING ? 1000 : e->type == T_BRUTE ? 150 : 50;
                }
            }
        }
    }
}

void game_update(const Input *in) {
    int ap = in->attack && !prev_attack, dp = in->dodge && !prev_dodge, sp = in->start && !prev_start;
    prev_attack = in->attack; prev_dodge = in->dodge; prev_start = in->start;
    frame_no++; state_t += DT;

    switch (state) {
    case ST_TITLE:     if (sp || ap) new_game(); break;
    case ST_PLAY:      update_play(in, ap, dp); break;
    case ST_DEAD:
    case ST_TRIAL_END: if (state_t > 1.0f && (sp || ap)) state = ST_TITLE; break;
    }
}

/* ----------------------------------------------------------- render */
static void draw_arena(int ox, int oy) {
    for (int y = 0; y < SCR_H; y++) {
        uint32_t c = 0x00000000u | ((8 + y / 40) << 16) | ((6 + y / 60) << 8) | (12 + y / 30);
        for (int x = 0; x < SCR_W; x++) FB[y * SCR_W + x] = c;
    }
    rect(40 + ox, 100 + oy, SCR_W - 80, SCR_H - 130, 0x00251d26);
    for (int i = 0; i < 9; i++) for (int j = 0; j < 5; j++)                 /* floor tiles */
        rect(60 + ox + i * 130, 120 + oy + j * 118, 126, 114, ((i + j) & 1) ? 0x002c2230 : 0x00302535);
    rect(40 + ox, 100 + oy, SCR_W - 80, 6, 0x00604050);                    /* walls */
    rect(40 + ox, SCR_H - 36 + oy, SCR_W - 80, 6, 0x00604050);
    rect(40 + ox, 100 + oy, 6, SCR_H - 130, 0x00604050);
    rect(SCR_W - 46 + ox, 100 + oy, 6, SCR_H - 130, 0x00604050);
    for (int i = 0; i < 4; i++) {                                           /* flickering torches */
        float fl = 10 + 3 * sinf(frame_no * 0.2f + i * 1.7f);
        int tx = 160 + i * 320 + ox;
        rect(tx - 3, 84 + oy, 6, 16, 0x00553322);
        circle((float)tx, 78.0f + oy, fl, 0x00ff8a20);
        circle((float)tx, 78.0f + oy, fl * 0.5f, 0x00ffe070);
    }
}

static void draw_hud(void) {
    char b[16];
    rect(30, 24, 324, 28, 0x00101018);
    rect(34, 28, (int)(316 * P.hp / 100.0f), 20, P.hp > 30 ? 0x00c02828 : 0x00ff5030);
    text(34, 58, 2, 0x00d0d0d0, "HEALTH");
    text(SCR_W - 330, 24, 3, 0x00e8c860, "SCORE");
    itoa_s(score, b); text(SCR_W - 190, 24, 3, 0x00ffffff, b);
    text(SCR_W - 330, 56, 2, 0x00d0d0d0, "WAVE");
    itoa_s(wave, b); text(SCR_W - 250, 56, 2, 0x00ffffff, b);
    text(SCR_W - 230, 56, 2, 0x00909090, "/3");
    text(30, SCR_H - 22, 2, 0x00909090, "SQUARE: AXE   CROSS: DODGE");
    text(SCR_W - 330, SCR_H - 22, 2, 0x00e04040, "TRIAL VERSION");
}

void game_render(uint32_t *fb) {
    FB = fb;
    int ox = shake > 0 ? (int)((frand() - 0.5f) * shake) : 0;
    int oy = shake > 0 ? (int)((frand() - 0.5f) * shake) : 0;
    draw_arena(ox, oy);

    if (state == ST_TITLE) {
        text_c(190, 12, 0x00c01818, "FEAR OF DEAD");
        text_c(185, 12, 0x00f0e0d0, "FEAR OF DEAD");
        text_c(300, 4, 0x00e8c860, "TRIAL VERSION");
        if ((frame_no / 30) & 1) text_c(480, 4, 0x00ffffff, "PRESS START");
        text_c(600, 2, 0x00909090, "LEFT STICK: MOVE   SQUARE: AXE   CROSS: DODGE");
        return;
    }

    for (int i = 0; i < MAX_ENEMY; i++) if (E[i].on) {                    /* enemies */
        Enemy *e = &E[i];
        uint32_t body = e->type == T_KING ? 0x00704090 : e->type == T_BRUTE ? 0x00406070 : 0x00508060;
        if (e->flash > 0) body = 0x00ffffff;
        circle(e->x + ox, e->y + oy + e->r * 0.8f, e->r * 0.9f, 0x00101010);
        circle(e->x + ox, e->y + oy, e->r, body);
        circle(e->x - e->r * 0.3f + ox, e->y - e->r * 0.2f + oy, e->r * 0.14f, 0x00ff2020);
        circle(e->x + e->r * 0.3f + ox, e->y - e->r * 0.2f + oy, e->r * 0.14f, 0x00ff2020);
        if (e->type != T_WRAITH) {
            rect((int)(e->x - e->r) + ox, (int)(e->y - e->r - 12) + oy, (int)(e->r * 2), 5, 0x00201010);
            rect((int)(e->x - e->r) + ox, (int)(e->y - e->r - 12) + oy, (int)(e->r * 2 * e->hp / e->maxhp), 5, 0x00d03030);
        }
    }

    /* player (original hero: hooded wanderer with a great axe) */
    if (state != ST_DEAD && !(P.inv > 0 && (frame_no / 3) & 1)) {
        float px_ = P.x + ox, py_ = P.y + oy;
        circle(px_, py_ + 14, 18, 0x00101010);
        circle(px_, py_, 18, P.roll_t > 0 ? 0x0090a0b0 : 0x00c0a070);
        circle(px_ + P.fx * 7, py_ + P.fy * 7, 8, 0x00402a1c);
        rect((int)(px_ + P.fx * 26) - 3, (int)(py_ + P.fy * 26) - 3, 6, 6, 0x00a0a0b0);
        if (P.atk_t > 0) {
            float k = 1.0f - P.atk_t / 0.22f;
            sector(px_, py_, P.fx, P.fy, 60 + k * 30, 125, 0.25f, 0x00dfeaff);
            sector(px_, py_, P.fx, P.fy, 95, 125, 0.25f, 0x00ffffff);
        }
    }
    draw_hud();

    if (state == ST_PLAY && wave_banner > 0) {
        char b[16]; itoa_s(wave, b);
        text_c(260, 6, 0x00f0e0d0, "WAVE");
        text(SCR_W / 2 + 70, 260, 6, 0x00f0e0d0, b);
    }
    if (state == ST_DEAD) {
        rect(0, 250, SCR_W, 200, 0x00000000);
        text_c(280, 8, 0x00c01818, "YOU DIED");
        text_c(380, 3, 0x00d0d0d0, "PRESS START");
    }
    if (state == ST_TRIAL_END) {
        char b[16]; itoa_s(score, b);
        rect(0, 200, SCR_W, 320, 0x00000000);
        text_c(220, 6, 0x00e8c860, "TRIAL COMPLETE");
        text_c(320, 3, 0x00f0e0d0, "THANKS FOR PLAYING FEAR OF DEAD");
        text_c(380, 3, 0x00ffffff, "FINAL SCORE");
        text_c(420, 4, 0x00ffffff, b);
        text_c(480, 2, 0x00909090, "FULL VERSION COMING SOON");
    }
}
