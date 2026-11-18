/* FEAR OF DEAD - PS3 glue (PSL1GHT). Software-rendered 720p framebuffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>
#include "game.h"

#define CB_SIZE       0x100000
#define HOSTBUF_SIZE  (32 * 1024 * 1024)

static gcmContextData *ctx;
static u32 *rsx_buf[2];
static u32  rsx_off[2];
static u32 *back;                 /* main-RAM back buffer */
static int  running = 1, first_flip = 1;

static void sys_cb(u64 status, u64 param, void *usrdata) {
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

static void wait_flip(void) {
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}

static void flip(int id) {
    if (!first_flip) wait_flip(); else gcmResetFlipStatus();
    if (gcmSetFlip(ctx, id) == 0) {
        rsxFlushBuffer(ctx);
        gcmSetWaitFlip(ctx);
    }
    first_flip = 0;
}

static int video_init(void) {
    void *host = memalign(1024 * 1024, HOSTBUF_SIZE);
    if (!host) return -1;
    rsxInit(&ctx, CB_SIZE, HOSTBUF_SIZE, host);

    videoState vs; videoConfiguration vc; videoResolution res;
    if (videoGetState(0, 0, &vs) != 0) return -1;
    if (vs.state != 0) return -1;
    videoGetResolution(vs.displayMode.resolution, &res);

    memset(&vc, 0, sizeof vc);
    vc.resolution = vs.displayMode.resolution;
    vc.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch      = res.width * sizeof(u32);
    vc.aspect     = vs.displayMode.aspect;
    if (videoConfigure(0, &vc, NULL, 0) != 0) return -1;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    /* NOTE: framebuffer is SCR_W x SCR_H (720p). Console must output 720p+. */
    for (int i = 0; i < 2; i++) {
        rsx_buf[i] = (u32 *)rsxMemalign(64, SCR_W * SCR_H * sizeof(u32));
        if (!rsx_buf[i]) return -1;
        if (rsxAddressToOffset(rsx_buf[i], &rsx_off[i]) != 0) return -1;
        if (gcmSetDisplayBuffer(i, rsx_off[i], SCR_W * sizeof(u32), SCR_W, SCR_H) != 0) return -1;
    }
    gcmResetFlipStatus();
    return 0;
}

static float stick(u16 v) {
    float f = ((int)v - 128) / 128.0f;
    return (f > -0.15f && f < 0.15f) ? 0.0f : f;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    padInfo pinfo; padData pdata;

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_cb, NULL);
    if (video_init() != 0) return 1;
    ioPadInit(7);

    back = (u32 *)malloc(SCR_W * SCR_H * sizeof(u32));
    if (!back) return 1;
    game_init();

    int cur = 0;
    while (running) {
        sysUtilCheckCallback();

        Input in; memset(&in, 0, sizeof in);
        ioPadGetInfo(&pinfo);
        if (pinfo.status[0]) {
            ioPadGetData(0, &pdata);
            in.lx = stick(pdata.ANA_L_H);
            in.ly = stick(pdata.ANA_L_V);
            if (pdata.BTN_LEFT)  in.lx = -1.0f;
            if (pdata.BTN_RIGHT) in.lx =  1.0f;
            if (pdata.BTN_UP)    in.ly = -1.0f;
            if (pdata.BTN_DOWN)  in.ly =  1.0f;
            in.attack = pdata.BTN_SQUARE;
            in.dodge  = pdata.BTN_CROSS;
            in.start  = pdata.BTN_START;
        }

        game_update(&in);
        game_render(back);
        memcpy(rsx_buf[cur], back, SCR_W * SCR_H * sizeof(u32));
        flip(cur);
        cur ^= 1;
    }

    gcmSetWaitFlip(ctx);
    rsxFinish(ctx, 1);
    ioPadEnd();
    free(back);
    return 0;
}
