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
#define MAP_W      20
#define MAP_H      12
#define FARM_C0    1
#define FARM_R0    1
#define FARM_W     4
#define FARM_H     3
#define DAY_FRAMES 900

static int S = 4;
#define TILE (16 * S)

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
static int pl_x = 0, pl_y = 0;

static const char *const world[MAP_H] = {
    "............HHHH....",
    ".FFFF.......HHHH....",
    ".FFFF...T...HHHH....",
    ".FFFF.......HHHH....",
    "....R........p......",
    ".........T...p......",
    "......pppppppp......",
    "......p..........www" "w",
    "......p...R.T.wwwwww",
    "..T...........wwwwww",
    ".....R........wwwwww",
    "..............wwwwww"
};

static const char *const spr_tuftA[16] = {
    "................",
    "................",
    "................",
    "................",
    "......h.........",
    "......h.h.......",
    ".....hGhGh......",
    "................",
    "................",
    "................",
    "................",
    "..........h.....",
    ".........hGh....",
    "........hGhGh...",
    "................",
    "................"
};

static const char *const spr_tuftB[16] = {
    "................",
    "................",
    "..h.............",
    "..h.h...........",
    ".hGhGh..........",
    "................",
    "................",
    "................",
    "................",
    "..............h.",
    ".............hGh",
    "............hGhG",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_flowerR[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "....r.......r...",
    "...rxr.....rxr..",
    "....r.......r...",
    "....k.......k...",
    "....k.......k...",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_flowerY[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "..y.........y...",
    ".yoy.......yoy..",
    "..y.........y...",
    "..k.........k...",
    "..k.........k...",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_dirt[16] = {
    "................",
    ".....e..........",
    "................",
    "..........D.....",
    "................",
    "..e.............",
    "................",
    "...........e....",
    "................",
    "......D.........",
    "................",
    "..............e.",
    "................",
    "....e.........D.",
    "................",
    "........e......."
};

static const char *const spr_waterA[16] = {
    "................",
    "..vv........vv..",
    "................",
    "......uu........",
    "................",
    "............uu..",
    "................",
    "...vv.......vv..",
    "................",
    "........uu......",
    "................",
    "..uu............",
    "................",
    "......vv......vv",
    "................",
    "..........uu...."
};

static const char *const spr_waterB[16] = {
    "................",
    "................",
    ".....vv.......vv",
    "................",
    "..uu......uu....",
    "................",
    "........vv......",
    "................",
    "...uu......vv...",
    "................",
    "..........uu....",
    "................",
    ".vv.........uu..",
    "................",
    "......vv........",
    "................"
};

static const char *const spr_canopy[16] = {
    "................",
    "......LLLL......",
    "....LLllllLL....",
    "...LllkkkkllL...",
    "..LllkkkkkklLL..",
    "..LlkkkkkkkklL..",
    ".LllkkkklkkkllL.",
    ".LlkkkklllkkklL.",
    ".LllkkllllklllL.",
    ".LlllllllllllLL.",
    ".LlllklllllllLL.",
    ".LLlllllllllllL.",
    "..LLllllllllLL..",
    "..LLLLllllLLLL..",
    "...LLLLLLLLLL...",
    "....LLLLLLLL...."
};

static const char *const spr_trunk[16] = {
    "......LLLL......",
    "......tttT......",
    "......tttT......",
    "......tttT......",
    ".....ttttTT.....",
    "....tttttTTT....",
    "...QQQQQQQQQQ...",
    "....QQQQQQQQ....",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_rock[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "......aaaa......",
    "....aaaaaaas....",
    "...aaassssssS...",
    "..aasssssssssS..",
    "..assssssssssSS.",
    "..sssssssssssSS.",
    "...SSSSSSSSSSS..",
    "....QQQQQQQQ....",
    "................",
    "................",
    "................"
};

static const char *const spr_seed[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......t..t......",
    "................",
    "................",
    "...t.......t....",
    "................",
    "........t.......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_sprout[16] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "......k..k......",
    ".....kk..kk.....",
    "......kLLk......",
    ".......LL.......",
    ".......LL.......",
    ".......LL.......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_grown[16] = {
    "................",
    "................",
    ".....kk..kk.....",
    "....kLLkkLLk....",
    "...kLlkLLkllLk..",
    "..kLllrLLkrllLk.",
    "..kLlrrllLllrrk.",
    "...kLlrllllrlk..",
    "....kLlLLLllk...",
    ".....kkLLLkk....",
    ".......LL.......",
    ".......LL.......",
    "................",
    "................",
    "................",
    "................"
};

static u32 pal(char ch)
{
    switch (ch) {
    case 'g': return 0x004DAA3C;
    case 'G': return 0x003F9632;
    case 'h': return 0x0066C84E;
    case 'd': return 0x008A5A2B;
    case 'D': return 0x006E4522;
    case 'e': return 0x00A97A45;
    case 'n': return 0x005A3C22;
    case 'N': return 0x0044301C;
    case 'w': return 0x003C7DD8;
    case 'u': return 0x002E66B8;
    case 'v': return 0x008CC4F5;
    case 't': return 0x006B3F1D;
    case 'T': return 0x004A2A12;
    case 'l': return 0x002F8A2F;
    case 'L': return 0x001E6B22;
    case 'k': return 0x0058B84C;
    case 'r': return 0x00D94040;
    case 'y': return 0x00F2D13A;
    case 'o': return 0x00F08A2A;
    case 'x': return 0x00F5F5F5;
    case 's': return 0x008E8E8E;
    case 'S': return 0x006A6A6A;
    case 'a': return 0x00B8B8B8;
    case 'b': return 0x008C3B2A;
    case 'B': return 0x006A2A1E;
    case 'c': return 0x00D8B77A;
    case 'C': return 0x00B8935A;
    case 'W': return 0x009AD0F0;
    case 'z': return 0x002A2A2A;
    case 'f': return 0x00F0C8A0;
    case 'm': return 0x007A4A22;
    case 'M': return 0x005A3416;
    case 'i': return 0x00D03030;
    case 'I': return 0x00A02020;
    case 'j': return 0x003A4A9A;
    case 'Q': return 0x002F7A28;
    default:  return 0x00FF00FF;
    }
}

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

static void wait_flip(void)
{
    int n = 0;
    while (gcmGetFlipStatus() != 0 && n < 250) {
        usleep(200);
        n++;
    }
    gcmResetFlipStatus();
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

static void art_rect(int ax, int ay, int aw, int ah, char ch)
{
    fill_rect(ax * S, ay * S, aw * S, ah * S, pal(ch));
}

static void prect(int x, int y, int w, int h, char ch)
{
    fill_rect(pl_x + x * S, pl_y + y * S, w * S, h * S, pal(ch));
}

static void draw_sprite(int sx, int sy, const char *const *spr)
{
    for (int j = 0; j < 16; j++) {
        const char *row = spr[j];
        if (!row)
            continue;
        for (int i = 0; i < 16 && row[i]; i++) {
            if (row[i] == '.')
                continue;
            fill_rect(sx + i * S, sy + j * S, S, S, pal(row[i]));
        }
    }
}

static void draw_base(void)
{
    u32 *buf = fb_addr[curr_fb];
    u32 cor = pal('g');
    u32 n = fb_width * fb_height;
    for (u32 i = 0; i < n; i++)
        buf[i] = cor;
}

static char tile_at(int c, int r)
{
    if (c < 0 || r < 0 || c >= MAP_W || r >= MAP_H)
        return '.';
    const char *row = world[r];
    for (int i = 0; i < c; i++)
        if (!row[i])
            return '.';
    return row[c] ? row[c] : '.';
}

static unsigned tile_hash(int c, int r)
{
    unsigned h = (unsigned)c * 374761393u + (unsigned)r * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void draw_soil(int sx, int sy, int wet)
{
    char base = wet ? 'n' : 'd';
    char dark = wet ? 'N' : 'D';
    char spec = wet ? 'D' : 'e';

    for (int j = 0; j < 16; j++) {
        if ((j & 3) == 0)
            fill_rect(sx, sy + j * S, TILE, S, pal(dark));
        else
            fill_rect(sx, sy + j * S, TILE, S, pal(base));
    }
    for (int k = 0; k < 6; k++) {
        int x = (k * 7 + 3) % 16;
        int y = ((k * 5 + 1) % 3) + 1 + (k % 3) * 4;
        fill_rect(sx + x * S, sy + y * S, S, S, pal(spec));
    }
}

static void draw_farm_tile(int fc, int fr, int sx, int sy)
{
    if (fc < 0 || fc >= FARM_W || fr < 0 || fr >= FARM_H)
        return;

    int st = farm_state[fr][fc];

    if (st == 0) {
        u32 borda = pal('h');
        fill_rect(sx, sy, TILE, S, borda);
        fill_rect(sx, sy + TILE - S, TILE, S, borda);
        fill_rect(sx, sy, S, TILE, borda);
        fill_rect(sx + TILE - S, sy, S, TILE, borda);
        return;
    }

    draw_soil(sx, sy, farm_water[fr][fc]);

    if (st == 2)
        draw_sprite(sx, sy, spr_seed);
    else if (st == 3)
        draw_sprite(sx, sy, spr_sprout);
    else if (st == 4)
        draw_sprite(sx, sy, spr_grown);
}

static void draw_ground(int frame)
{
    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;
    int wf = (frame / 30) & 1;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            char t = tile_at(c, r);
            int sx = c * TILE;
            int sy = r * TILE;

            switch (t) {
            case 'p':
                fill_rect(sx, sy, TILE, TILE, pal('d'));
                draw_sprite(sx, sy, spr_dirt);
                break;
            case 'w':
                fill_rect(sx, sy, TILE, TILE, pal('w'));
                draw_sprite(sx, sy, wf ? spr_waterB : spr_waterA);
                break;
            case 'F':
                draw_farm_tile(c - FARM_C0, r - FARM_R0, sx, sy);
                break;
            case '.': {
                unsigned h = tile_hash(c, r) % 100;
                if (h < 8)
                    draw_sprite(sx, sy, spr_tuftA);
                else if (h < 16)
                    draw_sprite(sx, sy, spr_tuftB);
                else if (h < 20)
                    draw_sprite(sx, sy, spr_flowerR);
                else if (h < 24)
                    draw_sprite(sx, sy, spr_flowerY);
                break;
            }
            case 'T':
                draw_sprite(sx, sy, spr_trunk);
                break;
            case 'R':
                draw_sprite(sx, sy, spr_rock);
                break;
            default:
                break;
            }
        }
    }
}

static void draw_canopies(void)
{
    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 2;

    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (tile_at(c, r) == 'T')
                draw_sprite(c * TILE, (r - 1) * TILE, spr_canopy);
}

static void draw_house(void)
{
    art_rect(236, 0, 8, 14, 'S');
    art_rect(235, 0, 10, 2, 'a');

    art_rect(188, 8, 72, 24, 'b');
    for (int i = 0; i < 6; i++)
        art_rect(188, 12 + i * 4, 72, 1, 'B');
    art_rect(188, 6, 72, 3, 'B');
    art_rect(188, 32, 72, 2, 'B');

    art_rect(192, 34, 64, 28, 'c');
    for (int i = 0; i < 5; i++)
        art_rect(192, 40 + i * 6, 64, 1, 'C');
    art_rect(192, 62, 64, 2, 'S');

    art_rect(210, 44, 12, 18, 'T');
    art_rect(211, 45, 10, 17, 't');
    art_rect(218, 54, 2, 2, 'y');

    art_rect(197, 42, 10, 9, 'T');
    art_rect(198, 43, 8, 7, 'W');
    art_rect(202, 43, 1, 7, 'T');
    art_rect(198, 46, 8, 1, 'T');

    art_rect(237, 42, 10, 9, 'T');
    art_rect(238, 43, 8, 7, 'W');
    art_rect(242, 43, 1, 7, 'T');
    art_rect(238, 46, 8, 1, 'T');
}

static void draw_player(int dir, int anim)
{
    int la = 5, lb = 5;
    if (anim == 1) la = 4;
    if (anim == 2) lb = 4;

    prect(1, 18, 10, 2, 'Q');

    if (dir == 0 || dir == 1) {
        prect(0, 9, 2, 5, 'I');
        prect(10, 9, 2, 5, 'I');
        prect(0, 14, 2, 1, 'f');
        prect(10, 14, 2, 1, 'f');
        prect(2, 8, 8, 6, 'i');
        prect(2, 13, 8, 1, 'I');
        prect(3, 14, 3, la, 'j');
        prect(6, 14, 3, lb, 'j');
        prect(3, 14 + la, 3, 1, 't');
        prect(6, 14 + lb, 3, 1, 't');
        if (dir == 0) {
            prect(2, 0, 8, 3, 'm');
            prect(2, 3, 1, 3, 'm');
            prect(9, 3, 1, 3, 'm');
            prect(3, 3, 6, 5, 'f');
            prect(4, 5, 1, 1, 'z');
            prect(7, 5, 1, 1, 'z');
        } else {
            prect(2, 0, 8, 8, 'm');
            prect(3, 6, 6, 2, 'M');
        }
    } else {
        int ax = (anim == 1) ? 3 : (anim == 2) ? 6 : 4;
        int bx = (anim == 1) ? 6 : (anim == 2) ? 3 : 4;

        prect(3, 8, 6, 6, 'i');
        prect(3, 13, 6, 1, 'I');
        prect(5, 9, 2, 5, 'I');
        prect(5, 14, 2, 1, 'f');
        prect(ax, 14, 3, 5, 'j');
        prect(ax, 19, 3, 1, 't');
        prect(bx, 14, 3, 4, 'j');
        prect(bx, 18, 3, 1, 't');

        prect(2, 0, 8, 3, 'm');
        if (dir == 3) {
            prect(2, 3, 4, 4, 'm');
            prect(6, 3, 4, 5, 'f');
            prect(8, 5, 1, 1, 'z');
        } else {
            prect(6, 3, 4, 4, 'm');
            prect(2, 3, 4, 5, 'f');
            prect(3, 5, 1, 1, 'z');
        }
    }
}

static void draw_hud(void)
{
    art_rect(1, 1, 66, 14, 'T');
    art_rect(2, 2, 64, 12, 'c');

    int n = (colheita > 15) ? 15 : colheita;
    for (int i = 0; i < n; i++)
        art_rect(3 + i * 4, 3, 3, 3, 'r');

    art_rect(3, 7, 60, 2, 'S');
    art_rect(3, 7, frames_dia * 60 / DAY_FRAMES, 2, 'o');

    int d = (dia > 15) ? 15 : dia;
    for (int i = 0; i < d; i++)
        art_rect(3 + i * 4, 10, 3, 3, 'w');
}

static int blocked_tile(char t)
{
    return t == 'T' || t == 'R' || t == 'H' || t == 'w';
}

static int colide(int px, int py)
{
    int x0 = px + 2 * S;
    int x1 = px + 10 * S - 1;
    int y0 = py + 16 * S;
    int y1 = py + 20 * S - 1;

    if (x0 < 0 || y0 < 0)
        return 0;

    return blocked_tile(tile_at(x0 / TILE, y0 / TILE)) ||
           blocked_tile(tile_at(x1 / TILE, y0 / TILE)) ||
           blocked_tile(tile_at(x0 / TILE, y1 / TILE)) ||
           blocked_tile(tile_at(x1 / TILE, y1 / TILE));
}

static void acao_fazenda(int px, int py, int cross, int square)
{
    int col = (px + 6 * S) / TILE - FARM_C0;
    int row = (py + 18 * S) / TILE - FARM_R0;

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
    int px, py;
    int dir = 0;
    int anim_t = 0;
    int frame_count = 0;
    int prev_cross = 0, prev_square = 0;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_video();

    S = fb_height / 180;
    if (S < 2)
        S = 2;

    px = 6 * TILE;
    py = 4 * TILE;

    curr_fb = 0;
    draw_base();
    curr_fb = 1;
    draw_base();
    gcmResetFlipStatus();
    gcmSetFlip(context, 1);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    curr_fb = 0;

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
        int sp = S * 3 / 2;

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
                mx += dx * S / 64;
            if (dy > 30 || dy < -30)
                my += dy * S / 64;

            if (paddata.BTN_LEFT)  mx -= sp;
            if (paddata.BTN_RIGHT) mx += sp;
            if (paddata.BTN_UP)    my -= sp;
            if (paddata.BTN_DOWN)  my += sp;
        }

        int moving = (mx != 0 || my != 0);
        if (moving) {
            if (abs(mx) > abs(my))
                dir = (mx < 0) ? 2 : 3;
            else
                dir = (my < 0) ? 1 : 0;
            anim_t++;
        } else {
            anim_t = 0;
        }

        int seq = (anim_t / 6) & 3;
        int anim = (seq == 1) ? 1 : (seq == 3) ? 2 : 0;

        if (!colide(px + mx, py))
            px += mx;
        if (!colide(px, py + my))
            py += my;

        int pw = 12 * S;
        int ph = 20 * S;
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px > (int)fb_width - pw)  px = fb_width - pw;
        if (py > (int)fb_height - ph) py = fb_height - ph;

        acao_fazenda(px, py, cross, square);

        frames_dia++;
        if (frames_dia >= DAY_FRAMES) {
            frames_dia = 0;
            passa_dia();
        }

        frame_count++;

        wait_flip();

        draw_base();
        draw_ground(frame_count);
        draw_house();
        pl_x = px;
        pl_y = py;
        draw_player(dir, anim);
        draw_canopies();
        draw_hud();
        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
