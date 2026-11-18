#ifndef GAME_H
#define GAME_H
#include <stdint.h>

#define SCR_W 1280
#define SCR_H 720

typedef struct {
    float lx, ly;   /* left stick, -1..1 */
    int attack;     /* Square  */
    int dodge;      /* Cross   */
    int start;      /* Start   */
} Input;

void game_init(void);
void game_update(const Input *in);   /* call once per frame @60fps */
void game_render(uint32_t *fb);      /* fb = SCR_W*SCR_H XRGB pixels */

#endif
