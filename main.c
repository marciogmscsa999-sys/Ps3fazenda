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
#define MARGEM    24

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

static void fill_rect_fb(int fb, int x, int y, int w, int h, u32 color)
{
    u32 *buf = fb_addr[fb];
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

static void fill_rect(int x, int y, int w, int h, u32 color)
{
    fill_rect_fb(curr_fb, x, y, w, h, color);
}

static u32 cor_do_tile(int c, int r)
{
    return ((r + c) % 2 == 0) ? 0x0045A83A : 0x003E9C34;
}

static void draw_scenery(int fb)
{
    fill_rect_fb(fb, 5 * TILE + 24, 3 * TILE + 32, 16, 40, 0x00654321);
    fill_rect_fb(fb, 5 * TILE, 3 * TILE - 20, 64, 56, 0x00206020);
    fill_rect_fb(fb, 10 * TILE, 6 * TILE, 48, 32, 0x00888888);
}

static void draw_full_map(int fb)
{
    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;

    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            fill_rect_fb(fb, c * TILE, r * TILE, TILE, TILE, cor_do_tile(c, r));

    draw_scenery(fb);
}

static void repaint_area(int fb, int ax, int ay, int aw, int ah)
{
    int c0 = ax / TILE;
    int r0 = ay / TILE;
    int c1 = (ax + aw) / TILE;
    int r1 = (ay + ah) / TILE;

    if (ax < 0) c0 = 0;
    if (ay < 0) r0 = 0;

    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            fill_rect_fb(fb, c * TILE, r * TILE, TILE, TILE, cor_do_tile(c, r));

    draw_scenery(fb);
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
    int old_px[2] = { 300, 300 };
    int old_py[2] = { 300, 300 };

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_video();

    draw_full_map(0);
    draw_full_map(1);

    while (running) {
        sysUtilCheckCallback();

        if (menu_aberto) {
            usleep(20000);
            continue;
        }

        if (redesenhar > 0) {
            draw_full_map(curr_fb);
            draw_player(px, py);
            old_px[curr_fb] = px;
            old_py[curr_fb] = py;
            flip_screen();
            redesenhar--;
            continue;
        }

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

        repaint_area(curr_fb,
                     old_px[curr_fb] - MARGEM, old_py[curr_fb] - MARGEM,
                     48 + 2 * MARGEM, 64 + 2 * MARGEM);
        draw_player(px, py);
        old_px[curr_fb] = px;
        old_py[curr_fb] = py;

        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
