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
#define PLAYER_W  48
#define PLAYER_H  64

static gcmContextData *context = NULL;
static void *host_addr = NULL;
static volatile int running = 1;
static volatile int menu_aberto = 0;
static volatile int redesenhar = 0;
static u32 curr_fb = 0;
static u32 fb_offset[2];
static u32 *fb_addr[2];
static u32 fb_pitch, fb_width, fb_height;

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME) {
        running = 0;
    } else if (status == SYSUTIL_MENU_OPEN) {
        menu_aberto = 1;
    } else if (status == SYSUTIL_MENU_CLOSE) {
        menu_aberto = 0;
        redesenhar = 10;
    }
}

static void init_video(void)
{
    videoState state;
    videoResolution res;
    videoConfiguration vconfig;

    host_addr = memalign(1024 * 1024, HOST_SIZE);
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
    u32 *buf = fb_addr[curr_fb];
    u32 verde1 = 0x0045A83A;
    u32 verde2 = 0x003E9C34;

    for (u32 y = 0; y < fb_height; y++) {
        int r = y / TILE;
        u32 *linha = buf + y * fb_width;
        for (u32 x = 0; x < fb_width; x++) {
            int c = x / TILE;
            linha[x] = (((r + c) & 1) == 0) ? verde1 : verde2;
        }
    }

    fill_rect(5 * TILE + 24, 3 * TILE + 32, 16, 40, 0x00654321);
    fill_rect(5 * TILE, 3 * TILE - 20, 64, 56, 0x00206020);
    fill_rect(10 * TILE, 6 * TILE, 48, 32, 0x00888888);
}

static void draw_player(int px, int py)
{
    fill_rect(px + 12, py, 24, 20, 0x00F0C8A0);
    fill_rect(px + 8, py + 20, 32, 28, 0x00D03030);
    fill_rect(px + 10, py + 48, 12, 16, 0x00303080);
    fill_rect(px + 26, py + 48, 12, 16, 0x00303080);
}

static void flip_screen(void)
{
    gcmSetFlip(context, curr_fb);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    curr_fb = !curr_fb;
}

static void shutdown_video(void)
{
    gcmSetWaitFlip(context);
    for (int i = 0; i < 2; i++)
        rsxFree(fb_addr[i]);
    rsxFinish(context, 1);
    free(host_addr);
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

        if (menu_aberto) {
            usleep(20000);
            continue;
        }

        if (redesenhar > 0)
            redesenhar--;

        ioPadGetInfo(&padinfo);

        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);

            if (paddata.BTN_START)
                running = 0;

            int dx = (int)paddata.ANA_L_H - 128;
            int dy = (int)paddata.ANA_L_V - 128;

            if (dx > 30 || dx < -30)
                px += dx / 16;
            if (dy > 30 || dy < -30)
                py += dy / 16;

            if (paddata.BTN_LEFT)  px -= 6;
            if (paddata.BTN_RIGHT) px += 6;
            if (paddata.BTN_UP)    py -= 6;
            if (paddata.BTN_DOWN)  py += 6;
        }

        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px > (int)fb_width - PLAYER_W)  px = fb_width - PLAYER_W;
        if (py > (int)fb_height - PLAYER_H) py = fb_height - PLAYER_H;

        draw_map();
        draw_player(px, py);
        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
