#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#define CB_SIZE   (1024 * 1024)
#define HOST_SIZE (32 * 1024 * 1024)

static gcmContextData *context = NULL;
static volatile int running = 1;
static u32 curr_fb = 0;
static u32 fb_offset[2];
static u32 *fb_addr[2];
static u32 fb_pitch, fb_width, fb_height;

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

static void init_video(void)
{
    videoState state;
    videoResolution res;
    videoConfiguration vconfig;

    void *host_addr = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);

    videoGetState(0, 0, &state);
    videoGetResolution(state.displayMode.resolution, &res);

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = res.width * 4;
    videoConfigure(0, &vconfig, NULL, 0);

    fb_width = res.width;
    fb_height = res.height;
    fb_pitch = res.width * 4;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < 2; i++) {
        fb_addr[i] = (u32 *)rsxMemalign(64, fb_height * fb_pitch);
        rsxAddressToOffset(fb_addr[i], &fb_offset[i]);
        gcmSetDisplayBuffer(i, fb_offset[i], fb_pitch, fb_width, fb_height);
    }
}

static void draw_screen(int px, int py)
{
    u32 *buf = fb_addr[curr_fb];

    for (u32 i = 0; i < fb_width * fb_height; i++)
        buf[i] = 0x00336699;

    for (int j = 0; j < 50; j++) {
        for (int i = 0; i < 50; i++) {
            int x = px + i;
            int y = py + j;
            if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height)
                buf[y * fb_width + x] = 0x0000FF00;
        }
    }
}

static void flip_screen(void)
{
    gcmSetFlip(context, curr_fb);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    curr_fb = !curr_fb;
}

int main(int argc, char *argv[])
{
    padInfo padinfo;
    padData paddata;
    int px = 200, py = 200;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_video();

    while (running) {
        sysUtilCheckCallback();
        ioPadGetInfo(&padinfo);

        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);

            if (paddata.BTN_START)
                running = 0;

            int dx = (int)paddata.ANA_L_H - 128;
            int dy = (int)paddata.ANA_L_V - 128;

            if (dx > 20 || dx < -20)
                px += dx / 16;
            if (dy > 20 || dy < -20)
                py += dy / 16;

            if (paddata.BTN_LEFT)  px -= 6;
            if (paddata.BTN_RIGHT) px += 6;
            if (paddata.BTN_UP)    py -= 6;
            if (paddata.BTN_DOWN)  py += 6;
        }

        draw_screen(px, py);
        flip_screen();
    }

    ioPadEnd();
    return 0;
}
