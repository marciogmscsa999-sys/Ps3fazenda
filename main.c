#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#define CB_SIZE    (1024 * 1024)
#define HOST_SIZE  (32 * 1024 * 1024)
#define TILE       64
#define PLAYER_W   48
#define PLAYER_H   64
#define FARM_C0    1
#define FARM_R0    1
#define FARM_W     4
#define FARM_H     3
#define DAY_FRAMES 900

static gcmContextData *context = NULL;
static void *host_addr = NULL;
static volatile int running = 1;
static volatile int menu_aberto = 0;
static volatile int redesenhar = 0;
static u32 curr_fb = 0;
static u32 fb_offset[2];
static u32 *fb_addr[2];
static u32 fb_pitch, fb_width, fb_height;

static unsigned char farm_state[FARM_H][FARM_W];
static unsigned char farm_water[FARM_H][FARM_W];
static int colheita = 0;
static int dia = 1;
static int frames_dia = 0;

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

static void draw_grass(void)
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
}

static void draw_scenery(void)
{
    fill_rect(5 * TILE + 24, 3 * TILE + 32, 16, 40, 0x00654321);
    fill_rect(5 * TILE, 3 * TILE - 20, 64, 56, 0x00206020);
    fill_rect(10 * TILE, 6 * TILE, 48, 32, 0x00888888);
}

static void draw_farm(void)
{
    for (int r = 0; r < FARM_H; r++) {
        for (int c = 0; c < FARM_W; c++) {
            int st = farm_state[r][c];
            int x = (FARM_C0 + c) * TILE;
            int y = (FARM_R0 + r) * TILE;

            if (st == 0) {
                u32 borda = 0x0075C265;
                fill_rect(x, y, TILE, 2, borda);
                fill_rect(x, y + TILE - 2, TILE, 2, borda);
                fill_rect(x, y, 2, TILE, borda);
                fill_rect(x + TILE - 2, y, 2, TILE, borda);
                continue;
            }

            u32 terra = farm_water[r][c] ? 0x00503018 : 0x00805030;
            fill_rect(x, y, TILE, TILE, terra);

            if (st == 2) {
                fill_rect(x + 28, y + 28, 8, 8, 0x00302010);
            } else if (st == 3) {
                fill_rect(x + 26, y + 20, 12, 28, 0x0040C040);
            } else if (st == 4) {
                fill_rect(x + 20, y + 12, 24, 40, 0x0030A030);
                fill_rect(x + 24, y + 8, 16, 16, 0x00E04040);
            }
        }
    }
}

static void draw_player(int px, int py)
{
    fill_rect(px + 12, py, 24, 20, 0x00F0C8A0);
    fill_rect(px + 8, py + 20, 32, 28, 0x00D03030);
    fill_rect(px + 10, py + 48, 12, 16, 0x00303080);
    fill_rect(px + 26, py + 48, 12, 16, 0x00303080);
}

static void draw_hud(void)
{
    int n = (colheita > 30) ? 30 : colheita;
    for (int i = 0; i < n; i++)
        fill_rect(8 + i * 16, 8, 12, 12, 0x00F0D020);

    fill_rect(8, 28, 200, 8, 0x00303030);
    fill_rect(8, 28, frames_dia * 200 / DAY_FRAMES, 8, 0x00FFFFFF);

    int d = (dia > 30) ? 30 : dia;
    for (int i = 0; i < d; i++)
        fill_rect(8 + i * 16, 44, 12, 12, 0x004080F0);
}

static int bate(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int colide(int px, int py)
{
    int fx = px + 8;
    int fy = py + 48;
    int fw = 32;
    int fh = 16;

    if (bate(fx, fy, fw, fh, 5 * TILE + 24, 3 * TILE + 32, 16, 40)) return 1;
    if (bate(fx, fy, fw, fh, 5 * TILE, 3 * TILE - 20, 64, 56)) return 1;
    if (bate(fx, fy, fw, fh, 10 * TILE, 6 * TILE, 48, 32)) return 1;
    return 0;
}

static void acao_fazenda(int px, int py, int cross, int square)
{
    int col = (px + PLAYER_W / 2) / TILE - FARM_C0;
    int row = (py + PLAYER_H - 8) / TILE - FARM_R0;

    if (col < 0 || col >= FARM_W || row < 0 || row >= FARM_H)
        return;

    unsigned char *st = &farm_state[row][col];

    if (cross) {
        if (*st == 0) {
            *st = 1;
        } else if (*st == 1) {
            *st = 2;
        } else if (*st == 4) {
            *st = 1;
            farm_water[row][col] = 0;
            colheita++;
        }
    }

    if (square) {
        if (*st >= 1 && *st <= 3)
            farm_water[row][col] = 1;
    }
}

static void passa_dia(void)
{
    dia++;
    for (int r = 0; r < FARM_H; r++) {
        for (int c = 0; c < FARM_W; c++) {
            if ((farm_state[r][c] == 2 || farm_state[r][c] == 3) && farm_water[r][c])
                farm_state[r][c]++;
            farm_water[r][c] = 0;
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
    int prev_cross = 0, prev_square = 0;

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

        int mx = 0, my = 0;
        int cross = 0, square = 0;

        ioPadGetInfo(&padinfo);

        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);

            if (paddata.BTN_START)
                running = 0;

            int c_now = paddata.BTN_CROSS;
            int s_now = paddata.BTN_SQUARE;
            cross = c_now && !prev_cross;
            square = s_now && !prev_square;
            prev_cross = c_now;
            prev_square = s_now;

            int dx = (int)paddata.ANA_L_H - 128;
            int dy = (int)paddata.ANA_L_V - 128;

            if (dx > 30 || dx < -30)
                mx += dx / 16;
            if (dy > 30 || dy < -30)
                my += dy / 16;

            if (paddata.BTN_LEFT)  mx -= 6;
            if (paddata.BTN_RIGHT) mx += 6;
            if (paddata.BTN_UP)    my -= 6;
            if (paddata.BTN_DOWN)  my += 6;
        }

        if (!colide(px + mx, py))
            px += mx;
        if (!colide(px, py + my))
            py += my;

        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px > (int)fb_width - PLAYER_W)  px = fb_width - PLAYER_W;
        if (py > (int)fb_height - PLAYER_H) py = fb_height - PLAYER_H;

        acao_fazenda(px, py, cross, square);

        frames_dia++;
        if (frames_dia >= DAY_FRAMES) {
            frames_dia = 0;
            passa_dia();
        }

        draw_grass();
        draw_farm();
        draw_scenery();
        draw_player(px, py);
        draw_hud();
        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
