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
#define DAY_FRAMES 5400
#define NSLOTS     12
#define ENERGY_MAX 100
#define COST_HOE   5
#define COST_CAN   3
#define COST_SEED  2

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
static u32 *cache = NULL;
static u32 *work = NULL;
static u32 *target = NULL;

static unsigned char farm_state[FARM_H][FARM_W];
static unsigned char farm_water[FARM_H][FARM_W];
static unsigned char farm_type[FARM_H][FARM_W];
static unsigned char farm_age[FARM_H][FARM_W];
static int dia = 1;
static int frames_dia = 0;
static int season = 0;
static int rebuild = 0;
static int slot = 0;
static int energia = ENERGY_MAX;
static int seeds[3] = { 5, 5, 5 };
static int crops[3] = { 0, 0, 0 };
static const int grow_need[3] = { 2, 1, 3 };
static const char emblem_ch[3] = { 'r', 'H', 'O' };
static const char season_col[4] = { 'K', 'y', 'O', 'q' };
static char emblem = 'r';
static int pl_x = 0, pl_y = 0;

static const u32 cs_g[4] = { 0x004DAA3C, 0x005DB438, 0x00A5A53A, 0x00E6EEF2 };
static const u32 cs_G[4] = { 0x003F9632, 0x004A9C2C, 0x008C8C2E, 0x00C8D6DE };
static const u32 cs_h[4] = { 0x0066C84E, 0x007AD04A, 0x00C2BE50, 0x00FFFFFF };
static const u32 cs_Q[4] = { 0x002F7A28, 0x002A7424, 0x007A7A28, 0x00B0C0CC };
static const u32 cs_l[4] = { 0x002F8A2F, 0x00258020, 0x00C8641E, 0x006E9C78 };
static const u32 cs_L[4] = { 0x001E6B22, 0x00186018, 0x008C3E12, 0x004A7258 };
static const u32 cs_k[4] = { 0x0058B84C, 0x004AA83E, 0x00E6A030, 0x00B8D4C0 };
static const u32 cs_w[4] = { 0x003C7DD8, 0x003C7DD8, 0x003C7DD8, 0x007FB2E0 };
static const u32 cs_u[4] = { 0x002E66B8, 0x002E66B8, 0x002E66B8, 0x005C94C8 };
static const u32 cs_v[4] = { 0x008CC4F5, 0x008CC4F5, 0x008CC4F5, 0x00C4E4FA };

static const unsigned char font3x5[10][5] = {
    { 7, 5, 5, 5, 7 },
    { 2, 6, 2, 2, 7 },
    { 7, 1, 7, 4, 7 },
    { 7, 1, 7, 1, 7 },
    { 5, 5, 7, 1, 1 },
    { 7, 4, 7, 1, 7 },
    { 7, 4, 7, 5, 7 },
    { 7, 1, 1, 1, 1 },
    { 7, 5, 7, 5, 7 },
    { 7, 5, 7, 1, 7 }
};

static const char *const world[MAP_H] = {
    "............HHHH....",
    ".FFFF.......HHHH....",
    ".FFFF...T...HHHH....",
    ".FFFF.......HHHH....",
    "....R........p......",
    ".........T...p......",
    "......pppppppp......",
    "......p.........wwww",
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

static const char *const spr_carrot[16] = {
    "................",
    "................",
    "......k..k......",
    ".....kLkkLk.....",
    "......kLLk......",
    ".......LL.......",
    "......oooo......",
    ".....oooooo.....",
    ".....oooooo.....",
    "......oooo......",
    ".......oo.......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const spr_pumpkin[16] = {
    "................",
    "................",
    "................",
    "................",
    ".......tt.......",
    "......tTt.......",
    "....oooooooo....",
    "...ooooyyoooo...",
    "..oooyyooyyooo..",
    "..oooyyooyyooo..",
    "..oooyyooyyooo..",
    "...ooyyooyyoo...",
    "....oooooooo....",
    ".....NNNNNN.....",
    "................",
    "................"
};

/* ---------- icones do inventario ---------- */

static const char *const spr_i_hoe[16] = {
    "................",
    "................",
    "..........aaaa..",
    ".........asssSS.",
    "........assSSS..",
    "........tsSS....",
    ".......ttT......",
    "......ttT.......",
    ".....ttT........",
    "....ttT.........",
    "...ttT..........",
    "..ttT...........",
    ".ttT............",
    ".tT.............",
    "................",
    "................"
};

static const char *const spr_i_can[16] = {
    "................",
    "................",
    "................",
    "............jj..",
    "...........jj...",
    "..........jqj...",
    ".........jqj....",
    "...jjjjjjjjj....",
    "..Sjqqqqqqqqj...",
    ".SSjqqqqqqqqj...",
    ".S.jqqqqqqqqj...",
    ".SSjqqqqqqqqj...",
    "..Sjjjjjjjjjj...",
    "....jjjjjjjj....",
    "................",
    "................"
};

static const char *const spr_i_pick[16] = {
    "................",
    "................",
    "..aa........aa..",
    ".assaaaaaaaassa.",
    "asSSsssssssSSSsa",
    ".a..SSSSSSSS..a.",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......tT.......",
    ".......TT.......",
    "................"
};

static const char *const spr_i_axe[16] = {
    "................",
    "................",
    "...aaaaa........",
    "..asssssStT.....",
    ".asssssssStT....",
    ".asssssssStT....",
    "..aSSSSSSStT....",
    "...aSSSSS.tT....",
    "..........tT....",
    "..........tT....",
    "..........tT....",
    "..........tT....",
    "..........tT....",
    "..........tT....",
    "..........TT....",
    "................"
};

static const char *const spr_i_scythe[16] = {
    "................",
    "................",
    ".......aaaaaa...",
    "......assssssa..",
    ".....aSSSSssssa.",
    "....tt.....aSSa.",
    "....tT.......aa.",
    "....tT..........",
    "....tT..........",
    "....tT..........",
    "....tT..........",
    "....tT..........",
    "....tT..........",
    "....tT..........",
    "....TT..........",
    "................"
};

static const char *const spr_i_packet[16] = {
    "................",
    "................",
    "...ZZZZZZZZZZ...",
    "...ZCCCCCCCCZ...",
    "...ZPPPPPPPPZ...",
    "...ZPPXXXXPPZ...",
    "...ZPXXXXXXPZ...",
    "...ZPXXXXXXPZ...",
    "...ZPPXXXXPPZ...",
    "...ZPPPFFPPPZ...",
    "...ZPPFKKFPPZ...",
    "...ZPPPPPPPPZ...",
    "...ZPPPPPPPPZ...",
    "...ZPPPPPPPPZ...",
    "...ZZZZZZZZZZ...",
    "................"
};

static const char *const spr_i_tomato[16] = {
    "................",
    ".......F........",
    "......FKFF......",
    ".....FFKKFF.....",
    "....rrrFFrrr....",
    "...rrrrrrrrrr...",
    "..rrRRrrrrrrrr..",
    "..rRRrrrrrrrrr..",
    "..rRrrrrrrrrrr..",
    "..rrrrrrrrrrrr..",
    "..rrrrrrrrrrII..",
    "...rrrrrrrrII...",
    "....IIIIIIII....",
    ".....IIIIII.....",
    "................",
    "................"
};

static const char *const spr_i_carrot[16] = {
    "................",
    "......K..K......",
    ".....KFK.KF.....",
    "......KFKF......",
    ".....HHHHHH.....",
    ".....HHOHHO.....",
    "....HHHHHHHO....",
    "....HHOHHHHO....",
    ".....HHHHHOO....",
    ".....HHOHHO.....",
    "......HHHHO.....",
    "......HOHO......",
    ".......HO.......",
    ".......O........",
    "................",
    "................"
};

static const char *const spr_i_pumpkin[16] = {
    "................",
    "................",
    ".......tt.......",
    "......tTt.......",
    "....OOOtOOOO....",
    "..OOHHOOOOHHOO..",
    ".OOHHHOOOOOHHOO.",
    ".OHHHOOHHOOOHHO.",
    ".OHHOOHHHHOOOHO.",
    ".OHHOOHHHHOOOHO.",
    ".OOHOOHHHHOOHOO.",
    ".OOOOOOHHOOOOOO.",
    "..OVVVVVVVVVVO..",
    "...VVVVVVVVVV...",
    "................",
    "................"
};

static const char *const spr_bolt[8] = {
    "...yyy.",
    "..yyy..",
    ".yyy...",
    ".yyyyy.",
    "...yyy.",
    "..yy...",
    "..y....",
    ".y....."
};

static const char *const *const item_sprite[NSLOTS] = {
    spr_i_hoe, spr_i_can, spr_i_pick, spr_i_axe, spr_i_scythe,
    spr_i_packet, spr_i_packet, spr_i_packet,
    spr_i_tomato, spr_i_carrot, spr_i_pumpkin, NULL
};

static u32 pal(char ch)
{
    switch (ch) {
    case 'g': return cs_g[season];
    case 'G': return cs_G[season];
    case 'h': return cs_h[season];
    case 'Q': return cs_Q[season];
    case 'l': return cs_l[season];
    case 'L': return cs_L[season];
    case 'k': return cs_k[season];
    case 'w': return cs_w[season];
    case 'u': return cs_u[season];
    case 'v': return cs_v[season];
    case 'd': return 0x008A5A2B;
    case 'D': return 0x006E4522;
    case 'e': return 0x00A97A45;
    case 'n': return 0x005A3C22;
    case 'N': return 0x0044301C;
    case 't': return 0x006B3F1D;
    case 'T': return 0x004A2A12;
    case 'r': return 0x00D94040;
    case 'R': return 0x00F26B5B;
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
    case 'q': return 0x005C9CE8;
    case 'E': return 0x0047C257;
    case 'U': return 0x007A5C3A;
    case 'Z': return 0x00302418;
    case 'F': return 0x00297A2A;
    case 'K': return 0x0062C050;
    case 'P': return 0x00F7E9C9;
    case 'H': return 0x00FFA640;
    case 'O': return 0x00D2691E;
    case 'V': return 0x00A8500E;
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
    u32 *buf = target;
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

static void shade_rect(int x, int y, int w, int h)
{
    u32 *buf = target;
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= (int)fb_height)
            continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= (int)fb_width)
                continue;
            u32 p = buf[yy * fb_width + xx];
            u32 r = ((p >> 16) & 0xFF) * 3 / 4;
            u32 g = ((p >> 8) & 0xFF) * 3 / 4;
            u32 b = (p & 0xFF) * 3 / 4;
            buf[yy * fb_width + xx] = (r << 16) | (g << 8) | b;
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
            char ch = row[i];
            if (ch == '.')
                continue;
            if (ch == 'X')
                ch = emblem;
            fill_rect(sx + i * S, sy + j * S, S, S, pal(ch));
        }
    }
}

static void draw_small(int ax, int ay, const char *const *rows, int n, char ch)
{
    for (int j = 0; j < n; j++) {
        const char *row = rows[j];
        for (int i = 0; row[i]; i++)
            if (row[i] != '.')
                art_rect(ax + i, ay + j, 1, 1, ch);
    }
}

static void draw_digit(int ax, int ay, int d, char ch, int sc)
{
    if (d < 0 || d > 9)
        return;
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 3; i++)
            if (font3x5[d][j] & (4 >> i))
                art_rect(ax + i * sc, ay + j * sc, sc, sc, ch);
}

static void draw_count(int rx, int ry, int n)
{
    if (n <= 0)
        return;
    if (n > 99)
        n = 99;

    int two = (n >= 10);
    int x = rx - (two ? 7 : 3);

    for (int pass = 0; pass < 2; pass++) {
        int o = (pass == 0) ? 1 : 0;
        char ch = (pass == 0) ? 'Z' : 'x';
        if (two) {
            draw_digit(x + o, ry + o, n / 10, ch, 1);
            draw_digit(x + 4 + o, ry + o, n % 10, ch, 1);
        } else {
            draw_digit(x + o, ry + o, n, ch, 1);
        }
    }
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

    if (st == 2) {
        int t = farm_type[fr][fc];
        int age = farm_age[fr][fc];

        if (age == 0)
            draw_sprite(sx, sy, spr_seed);
        else if (age < grow_need[t])
            draw_sprite(sx, sy, spr_sprout);
        else if (t == 0)
            draw_sprite(sx, sy, spr_grown);
        else if (t == 1)
            draw_sprite(sx, sy, spr_carrot);
        else
            draw_sprite(sx, sy, spr_pumpkin);
    }
}

static void draw_house(void)
{
    shade_rect(190 * S, 64 * S, 80 * S, 4 * S);
    shade_rect(260 * S, 34 * S, 4 * S, 30 * S);

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
    art_rect(192, 34, 2, 28, 'x');
    art_rect(254, 34, 2, 28, 'C');

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

static void draw_static_world(void)
{
    u32 *buf = target;
    u32 cor = pal('g');
    u32 n = fb_width * fb_height;
    for (u32 i = 0; i < n; i++)
        buf[i] = cor;

    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;

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
            case '.': {
                unsigned h = tile_hash(c, r) % 100;
                if (h < 8)
                    draw_sprite(sx, sy, spr_tuftA);
                else if (h < 16)
                    draw_sprite(sx, sy, spr_tuftB);
                else if (season < 2 && h < 20)
                    draw_sprite(sx, sy, spr_flowerR);
                else if (season < 2 && h < 24)
                    draw_sprite(sx, sy, spr_flowerY);
                break;
            }
            case 'T':
                shade_rect(sx + 1 * S, sy + 8 * S, 14 * S, 6 * S);
                draw_sprite(sx, sy, spr_trunk);
                break;
            case 'R':
                shade_rect(sx + 1 * S, sy + 11 * S, 15 * S, 4 * S);
                draw_sprite(sx, sy, spr_rock);
                break;
            default:
                break;
            }
        }
    }

    draw_house();
}

static void draw_dynamic(int frame)
{
    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;
    int wf = (frame / 30) & 1;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            char t = tile_at(c, r);
            int sx = c * TILE;
            int sy = r * TILE;

            if (t == 'w') {
                fill_rect(sx, sy, TILE, TILE, pal('w'));
                draw_sprite(sx, sy, wf ? spr_waterB : spr_waterA);
            } else if (t == 'F') {
                draw_farm_tile(c - FARM_C0, r - FARM_R0, sx, sy);
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

static void draw_player(int dir, int anim)
{
    int la = 5, lb = 5;
    if (anim == 1) la = 4;
    if (anim == 2) lb = 4;

    shade_rect(pl_x + 1 * S, pl_y + 18 * S, 10 * S, 2 * S);

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

static int nivel_noite(int minutes)
{
    int h = minutes / 60;
    if (h < 7) return 1;
    if (h < 17) return 0;
    if (h < 19) return 1;
    if (h < 21) return 2;
    return 3;
}

static void darken_frame(int level)
{
    if (level <= 0)
        return;

    u32 *buf = work;
    u32 n = fb_width * fb_height;

    if (level == 1) {
        for (u32 i = 0; i < n; i++) {
            u32 p = buf[i];
            buf[i] = ((p >> 1) & 0x007F7F7F) + ((p >> 2) & 0x003F3F3F);
        }
    } else if (level == 2) {
        for (u32 i = 0; i < n; i++) {
            u32 p = buf[i];
            buf[i] = ((p >> 1) & 0x007F7F7F) + 0x00000008;
        }
    } else {
        for (u32 i = 0; i < n; i++) {
            u32 p = buf[i];
            buf[i] = ((p >> 2) & 0x003F3F3F) + ((p >> 4) & 0x000F0F0F) + 0x0000000A;
        }
    }
}

/* relogio e data, canto superior direito */
static void draw_hud(int minutes)
{
    int aw = (int)fb_width / S;
    int pw = 42, ph = 33;
    int x0 = aw - pw - 4;
    int y0 = 4;

    art_rect(x0, y0, pw, ph, 'T');
    art_rect(x0 + 1, y0 + 1, pw - 2, ph - 2, 'N');

    int h = minutes / 60;
    int m = minutes % 60;

    draw_digit(x0 + 5, y0 + 4, h / 10, 'x', 2);
    draw_digit(x0 + 13, y0 + 4, h % 10, 'x', 2);
    art_rect(x0 + 21, y0 + 7, 2, 2, 'c');
    art_rect(x0 + 21, y0 + 11, 2, 2, 'c');
    draw_digit(x0 + 25, y0 + 4, m / 10, 'x', 2);
    draw_digit(x0 + 33, y0 + 4, m % 10, 'x', 2);

    art_rect(x0 + 4, y0 + 17, pw - 8, 1, 'U');

    char sc = season_col[season];
    art_rect(x0 + 6, y0 + 21, 5, 7, sc);
    art_rect(x0 + 5, y0 + 22, 7, 5, sc);

    int d = dia;
    if (d > 99) d = 99;
    draw_digit(x0 + 17, y0 + 20, d / 10, 'c', 2);
    draw_digit(x0 + 25, y0 + 20, d % 10, 'c', 2);
}

/* barra de energia, canto inferior direito */
static void draw_energy(void)
{
    int aw = (int)fb_width / S;
    int ah = (int)fb_height / S;
    int bw = 10, bh = 52;
    int x = aw - bw - 4;
    int y = ah - bh - 4;

    draw_small(x + 2, y - 10, spr_bolt, 8, 'y');

    art_rect(x, y, bw, bh, 'T');
    art_rect(x + 1, y + 1, bw - 2, bh - 2, 'N');

    int ih = bh - 4;
    int fh = energia * ih / ENERGY_MAX;
    if (fh < 0) fh = 0;
    if (fh > ih) fh = ih;

    char ch = (energia > 50) ? 'E' : (energia > 25) ? 'y' : 'r';
    if (fh > 0) {
        art_rect(x + 2, y + 2 + (ih - fh), bw - 4, fh, ch);
        art_rect(x + 2, y + 2 + (ih - fh), 1, fh, 'K');
    }
}

/* inventario: 12 espacos, igual ao inicio do Stardew */
static void draw_hotbar(void)
{
    int aw = (int)fb_width / S;
    int ah = (int)fb_height / S;
    int pitch = 19;
    int hw = NSLOTS * pitch - 1 + 6;
    int ex = aw - 10 - 4;
    int hx = (aw - hw) / 2;

    if (hx + hw > ex - 3)
        hx = ex - 3 - hw;
    if (hx < 0)
        hx = 0;

    int hy = ah - 24 - 4;

    art_rect(hx, hy, hw, 24, 'T');
    art_rect(hx + 1, hy + 1, hw - 2, 22, 'N');

    for (int i = 0; i < NSLOTS; i++) {
        int x = hx + 3 + i * pitch;
        int y = hy + 3;
        int sel = (i == slot);

        art_rect(x, y, 18, 18, sel ? 'y' : 'T');
        art_rect(x + 1, y + 1, 16, 16, sel ? 'C' : 'U');

        int cnt = -1;
        if (i >= 5 && i <= 7)
            cnt = seeds[i - 5];
        else if (i >= 8 && i <= 10)
            cnt = crops[i - 8];

        if (item_sprite[i] && cnt != 0) {
            if (i >= 5 && i <= 7)
                emblem = emblem_ch[i - 5];
            draw_sprite((x + 1) * S, (y + 1) * S, item_sprite[i]);
        }
        if (cnt > 0)
            draw_count(x + 17, y + 11, cnt);
    }
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

static void acao_fazenda(int px, int py)
{
    int col = (px + 6 * S) / TILE - FARM_C0;
    int row = (py + 18 * S) / TILE - FARM_R0;

    if (col < 0 || col >= FARM_W || row < 0 || row >= FARM_H)
        return;

    unsigned char *st = &farm_state[row][col];

    if (*st == 2 && farm_age[row][col] >= grow_need[farm_type[row][col]]) {
        crops[farm_type[row][col]]++;
        *st = 1;
        farm_water[row][col] = 0;
        farm_age[row][col] = 0;
        return;
    }

    if (slot == 0) {
        if (*st == 0 && energia >= COST_HOE) {
            *st = 1;
            energia -= COST_HOE;
        }
    } else if (slot == 1) {
        if (*st >= 1 && !farm_water[row][col] && energia >= COST_CAN) {
            farm_water[row][col] = 1;
            energia -= COST_CAN;
        }
    } else if (slot >= 5 && slot <= 7) {
        int t = slot - 5;
        if (*st == 1 && seeds[t] > 0 && energia >= COST_SEED) {
            seeds[t]--;
            *st = 2;
            farm_type[row][col] = t;
            farm_age[row][col] = 0;
            energia -= COST_SEED;
        }
    }
}

static void passa_dia(void)
{
    dia++;
    energia = ENERGY_MAX;

    for (int r = 0; r < FARM_H; r++) {
        for (int c = 0; c < FARM_W; c++) {
            if (farm_state[r][c] == 2 && farm_water[r][c] && farm_age[r][c] < 250)
                farm_age[r][c]++;
            farm_water[r][c] = 0;
        }
    }

    int ns = ((dia - 1) / 4) & 3;
    if (ns != season) {
        season = ns;
        rebuild = 1;
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
    if (cache)
        free(cache);
    if (work)
        free(work);
}

int main(int argc, char *argv[])
{
    padInfo padinfo;
    padData paddata;
    int px, py;
    int dir = 0;
    int anim_t = 0;
    int frame_count = 0;
    int prev_cross = 0, prev_l1 = 0, prev_r1 = 0, prev_sel = 0;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_video();

    S = fb_height / 180;
    if (S < 2)
        S = 2;

    px = 6 * TILE;
    py = 4 * TILE;

    size_t bytes = (size_t)fb_width * fb_height * 4;
    cache = (u32 *)memalign(128, bytes);
    work = (u32 *)memalign(128, bytes);

    if (cache && work) {
        target = cache;
        draw_static_world();
        for (int i = 0; i < 2; i++)
            memcpy(fb_addr[i], cache, bytes);
    } else {
        running = 0;
        for (int i = 0; i < 2; i++)
            memset(fb_addr[i], 0, bytes);
    }

    curr_fb = 0;
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
        int usar = 0;
        int sp = S * 3 / 2;

        ioPadGetInfo(&padinfo);

        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);

            if (paddata.BTN_START)
                running = 0;

            int c_now = paddata.BTN_CROSS;
            int l1 = paddata.BTN_L1;
            int r1 = paddata.BTN_R1;
            int sel = paddata.BTN_SELECT;

            usar = c_now && !prev_cross;
            if (r1 && !prev_r1)
                slot = (slot + 1) % NSLOTS;
            if (l1 && !prev_l1)
                slot = (slot + NSLOTS - 1) % NSLOTS;
            if (sel && !prev_sel)
                frames_dia = DAY_FRAMES;

            prev_cross = c_now;
            prev_l1 = l1;
            prev_r1 = r1;
            prev_sel = sel;

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

        if (usar)
            acao_fazenda(px, py);

        frames_dia++;
        if (frames_dia >= DAY_FRAMES) {
            frames_dia = 0;
            passa_dia();
        }

        if (rebuild) {
            rebuild = 0;
            target = cache;
            draw_static_world();
        }

        frame_count++;

        int minutes = 360 + frames_dia * 1080 / DAY_FRAMES;
        int nivel = nivel_noite(minutes);

        wait_flip();

        memcpy(work, cache, bytes);
        target = work;

        draw_dynamic(frame_count);
        pl_x = px;
        pl_y = py;
        draw_player(dir, anim);
        draw_canopies();
        darken_frame(nivel);
        draw_hud(minutes);
        draw_energy();
        draw_hotbar();

        memcpy(fb_addr[curr_fb], work, bytes);
        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
