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
#define TILE      64

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

static void fill_rect(int x, int y, int w, int h, u32 color)
{
    u32 *buf = fb_addr[curr_fb];
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= (int)fb_height)
            continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= (int)fb_width)
                continue;
            buf[yy * fb_width + xx] = color;
        }
    }
}

static void draw_map(void)
{
    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            u32 color = ((r + c) % 2 == 0) ? 0x0045A83A : 0x003E9C34;
            fill_rect(c * TILE, r * TILE, TILE, TILE, color);
        }
    }

    /* arvore: tronco e copa */
    fill_rect(5 * TILE + 24, 3 * TILE + 32, 16, 40, 0x00654321);
    fill_rect(5 * TILE, 3 * TILE - 20, 64, 56, 0x00206020);

    /* pedra */
    fill_rect(10 * TILE, 6 * TILE, 48, 32, 0x00888888);
}

static void draw_player(int px, int py)
{
    fill_rect(px + 12, py, 24, 20, 0x00F0C8A0);       /* cabeca */
    fill_rect(px + 8, py + 20, 32, 28, 0x00D03030);   /* corpo */
    fill_rect(px + 10, py + 48, 12, 16, 0x00303080);  /* perna esq */
    fill_rect(px + 26, py + 48, 12, 16, 0x00303080);  /* perna dir */
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
    int px = 300, py = 300;

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

        draw_map();
        draw_player(px, py);
        flip_screen();
    }

    ioPadEnd();
    return 0;
}
