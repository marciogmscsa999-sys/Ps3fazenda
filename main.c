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

/* Tempo: 20 horas de jogo = 6:00 as 2:00
   1 hora real = ~42s. Para simplificar: 10 min de jogo = ~7s reais.
   A 60fps, 7s = 420 frames. Entao 10 min de jogo = 420 frames.
   1 dia = 120 * 420 = 50400 frames. Mas isso e muito.
   Vou usar uma escala mais rapida para o jogo nao ficar eterno:
   10 min de jogo = 60 frames (1s real). 1 hora = 360 frames (6s).
   1 dia (20h) = 7200 frames (2 min reais). Ajustavel. */

#define FRAMES_PER_10MIN 30
#define FRAMES_PER_HOUR  (6 * FRAMES_PER_10MIN)

#define WATER_MAX  20
#define ENERGY_MAX 100

static int S = 4;
#define TILE (16 * S)

static gcmContextData *context = NULL;
static void *host_addr = NULL;
static volatile int running = 1;
static volatile int menu_aberto = 0;
static u32 curr_fb = 0;
static u32 fb_offset[2];
static u32 *fb_addr[2];
static u32 fb_pitch, fb_width, fb_height;
static u32 *cache = NULL;
static u32 *target = NULL;

/* Estado do jogo */
static unsigned char farm_state[FARM_H][FARM_W];
static unsigned char farm_water[FARM_H][FARM_W];
static int colheita = 0;
static int moedas = 0;
static int energia = ENERGY_MAX;
static int agua_regador = WATER_MAX;
static int dia = 1;
static int pl_x = 0, pl_y = 0;
static int flash_timer = 0;
static int chovendo = 0;
static int chuva_timer = 0;
static int estacao = 0; /* 0=Primavera 1=Verao 2=Outono 3=Inverno */
static int hoe_level = 1;
static int can_level = 1;
static int exausto = 0;

/* Tempo */
static int tempo_frames = 0;     /* frames desde 6:00 */
static int hora_atual = 6;       /* 6 a 26 (26 = 2:00) */
static int minuto_atual = 0;
static int escuro = 0;           /* 0 a 100, quanto escurecer */
static int desmaiou = 0;

static const char *const world[MAP_H] = {
    "............HHHH....",
    ".FFFF.......HHHH....",
    ".FFFF..T....HHHH....",
    ".FFFF.......HHHH....",
    "....R......V.p......",
    "........O....p......",
    "......pppppppp......",
    "......p..........www",
    "......p...R.T.wwwwww",
    "..T...........wwwwww",
    ".....R........wwwwww",
    "..............wwwwww"
};

/* ===== SPRITES (mantidos) ===== */
static const char *const spr_tuftA[16] = {
    "................","................","................","................",
    "......h.........","......h.h.......",".....hGhGh......","................",
    "................","................","................","..........h.....",
    ".........hGh....","........hGhGh...","................","................"
};
static const char *const spr_tuftB[16] = {
    "................","................","..h.............","..h.h...........",
    ".hGhGh..........","................","................","................",
    "................","..............h.",".............hGh","............hGhG",
    "................","................","................","................"
};
static const char *const spr_flowerR[16] = {
    "................","................","................","................",
    "................","....r.......r...","...rxr.....rxr..","....r.......r...",
    "....k.......k...","....k.......k...","................","................",
    "................","................","................","................"
};
static const char *const spr_flowerY[16] = {
    "................","................","................","................",
    "................","................","................","..y.........y...",
    ".yoy.......yoy..","..y.........y...","..k.........k...","..k.........k...",
    "................","................","................","................"
};
static const char *const spr_dirt[16] = {
    "................",".....e..........","................","..........D.....",
    "................","..e.............","................","...........e....",
    "................","......D.........","................","..............e.",
    "................","....e.........D.","................","........e......."
};
static const char *const spr_waterA[16] = {
    "................","..vv........vv..","................","......uu........",
    "................","............uu..","................","...vv.......vv..",
    "................","........uu......","................","..uu............",
    "................","......vv......vv","................","..........uu...."
};
static const char *const spr_waterB[16] = {
    "................","................",".....vv.......vv","................",
    "..uu......uu....","................","........vv......","................",
    "...uu......vv...","................","..........uu....","................",
    ".vv.........uu..","................","......vv........","................"
};
static const char *const spr_canopy[16] = {
    "................","......LLLL......","....LLllllLL....","...LllkkkkllL...",
    "..LllkkkkkklLL..","..LlkkkkkkkklL..",".LllkkkklkkkllL.",".LlkkkklllkkklL.",
    ".LllkkllllklllL.",".LlllllllllllLL.",".LlllklllllllLL.",".LLlllllllllllL.",
    "..LLllllllllLL..","..LLLLllllLLLL..","...LLLLLLLLLL...","....LLLLLLLL...."
};
static const char *const spr_trunk[16] = {
    "......LLLL......","......tttT......","......tttT......","......tttT......",
    ".....ttttTT.....","....tttttTTT....","...QQQQQQQQQQ...","....QQQQQQQQ....",
    "................","................","................","................",
    "................","................","................","................"
};
static const char *const spr_rock[16] = {
    "................","................","................","................",
    "................","......aaaa......","....aaaaaaas....","...aaassssssS...",
    "..aasssssssssS..","..assssssssssSS.","..sssssssssssSS.","...SSSSSSSSSSS..",
    "....QQQQQQQQ....","................","................","................"
};
static const char *const spr_seed[16] = {
    "................","................","................","................",
    "................","................","......t..t......","................",
    "................","...t.......t....","................","........t.......",
    "................","................","................","................"
};
static const char *const spr_sprout[16] = {
    "................","................","................","................",
    "................","......k..k......",".....kk..kk.....","......kLLk......",
    ".......LL.......",".......LL.......",".......LL.......","................",
    "................","................","................","................"
};
static const char *const spr_grown[16] = {
    "................","................",".....kk..kk.....","....kLLkkLLk....",
    "...kLlkLLkllLk..","..kLllrLLkrllLk.","..kLlrrllLllrrk.","...kLlrllllrlk..",
    "....kLlLLLllk...",".....kkLLLkk....",".......LL.......",".......LL.......",
    "................","................","................","................"
};
static const char *const spr_well[16] = {
    "................","................","...ssssssssss...","..saaaaaaaaaas..",
    "..saSSSSSSSSas..","..saSuuuuuuSas..","..saSuuuuuuSas..","..saSSSSSSSSas..",
    "..saaaaaaaaaas..","...ssssssssss...","....s......s....","....s......s....",
    "....s......s....","................","................","................"
};
static const char *const spr_bush[16] = {
    "................","................","......kLLk......","....kLlkklLk....",
    "...kLllkklllLk..","..kLllkrrklllLk.","..kLllkrrklllLk.","..kLllkkklllLLk.",
    "...kLlrrlllLLk..","....kLlrllLLk...",".....kLLLLLk....","......kkkk......",
    "................","................","................","................"
};

static u32 pal(char ch)
{
    switch (ch) {
    case 'g': return 0x004DAA3C; case 'G': return 0x003F9632;
    case 'h': return 0x0066C84E; case 'd': return 0x008A5A2B;
    case 'D': return 0x006E4522; case 'e': return 0x00A97A45;
    case 'n': return 0x005A3C22; case 'N': return 0x0044301C;
    case 'w': return 0x003C7DD8; case 'u': return 0x002E66B8;
    case 'v': return 0x008CC4F5; case 't': return 0x006B3F1D;
    case 'T': return 0x004A2A12; case 'l': return 0x002F8A2F;
    case 'L': return 0x001E6B22; case 'k': return 0x0058B84C;
    case 'r': return 0x00D94040; case 'y': return 0x00F2D13A;
    case 'o': return 0x00F08A2A; case 'x': return 0x00F5F5F5;
    case 's': return 0x008E8E8E; case 'S': return 0x006A6A6A;
    case 'a': return 0x00B8B8B8; case 'b': return 0x008C3B2A;
    case 'B': return 0x006A2A1E; case 'c': return 0x00D8B77A;
    case 'C': return 0x00B8935A; case 'W': return 0x009AD0F0;
    case 'z': return 0x002A2A2A; case 'f': return 0x00F0C8A0;
    case 'm': return 0x007A4A22; case 'M': return 0x005A3416;
    case 'i': return 0x00D03030; case 'I': return 0x00A02020;
    case 'j': return 0x003A4A9A; case 'Q': return 0x002F7A28;
    case 'P': return 0x00387A2E;
    case '1': return 0x0055AA33; case '2': return 0x0088CC44;
    case '3': return 0x00CC8833; case '4': return 0x00DDDDEE;
    default:  return 0x00FF00FF;
    }
}

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
    else if (status == SYSUTIL_MENU_OPEN) menu_aberto = 1;
    else if (status == SYSUTIL_MENU_CLOSE) menu_aberto = 0;
}

static void wait_flip(void)
{
    int n = 0;
    while (gcmGetFlipStatus() != 0 && n < 250) { usleep(200); n++; }
    gcmResetFlipStatus();
}

static void init_video(void)
{
    videoState state; videoResolution res; videoConfiguration vconfig;
    host_addr = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);
    videoGetState(0, 0, &state);
    videoGetResolution(state.displayMode.resolution, &res);
    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = res.width * 4;
    videoConfigure(0, &vconfig, NULL, 0);
    fb_width = res.width; fb_height = res.height; fb_pitch = res.width * 4;
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
        if (yy < 0 || yy >= (int)fb_height) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= (int)fb_width) continue;
            buf[yy * fb_width + xx] = color;
        }
    }
}

static void shade_rect(int x, int y, int w, int h)
{
    u32 *buf = target;
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= (int)fb_height) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= (int)fb_width) continue;
            u32 p = buf[yy * fb_width + xx];
            u32 r = ((p >> 16) & 0xFF) * 3 / 4;
            u32 g = ((p >> 8) & 0xFF) * 3 / 4;
            u32 b = (p & 0xFF) * 3 / 4;
            buf[yy * fb_width + xx] = (r << 16) | (g << 8) | b;
        }
    }
}

/* Escurece a tela inteira para simular noite */
static void darken_screen(int amount)
{
    if (amount <= 0) return;
    u32 *buf = target;
    u32 n = fb_width * fb_height;
    for (u32 i = 0; i < n; i++) {
        u32 p = buf[i];
        u32 r = ((p >> 16) & 0xFF) * (100 - amount) / 100;
        u32 g = ((p >> 8) & 0xFF) * (100 - amount) / 100;
        u32 b = (p & 0xFF) * (100 - amount) / 100;
        buf[i] = (r << 16) | (g << 8) | b;
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
        if (!row) continue;
        for (int i = 0; i < 16 && row[i]; i++) {
            if (row[i] == '.') continue;
            fill_rect(sx + i * S, sy + j * S, S, S, pal(row[i]));
        }
    }
}

static char tile_at(int c, int r)
{
    if (c < 0 || r < 0 || c >= MAP_W || r >= MAP_H) return '.';
    const char *row = world[r];
    for (int i = 0; i < c; i++) if (!row[i]) return '.';
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
        if ((j & 3) == 0) fill_rect(sx, sy + j * S, TILE, S, pal(dark));
        else fill_rect(sx, sy + j * S, TILE, S, pal(base));
    }
    for (int k = 0; k < 6; k++) {
        int x = (k * 7 + 3) % 16;
        int y = ((k * 5 + 1) % 3) + 1 + (k % 3) * 4;
        fill_rect(sx + x * S, sy + y * S, S, S, pal(spec));
    }
}

static void draw_farm_tile(int fc, int fr, int sx, int sy)
{
    if (fc < 0 || fc >= FARM_W || fr < 0 || fr >= FARM_H) return;
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
    if (st == 2) draw_sprite(sx, sy, spr_seed);
    else if (st == 3) draw_sprite(sx, sy, spr_sprout);
    else if (st == 4) draw_sprite(sx, sy, spr_grown);
}

static void draw_house(void)
{
    shade_rect(190 * S, 64 * S, 80 * S, 4 * S);
    shade_rect(260 * S, 34 * S, 4 * S, 30 * S);
    art_rect(236, 0, 8, 14, 'S'); art_rect(235, 0, 10, 2, 'a');
    art_rect(188, 8, 72, 24, 'b');
    for (int i = 0; i < 6; i++) art_rect(188, 12 + i * 4, 72, 1, 'B');
    art_rect(188, 6, 72, 3, 'B'); art_rect(188, 32, 72, 2, 'B');
    art_rect(192, 34, 64, 28, 'c');
    for (int i = 0; i < 5; i++) art_rect(192, 40 + i * 6, 64, 1, 'C');
    art_rect(192, 62, 64, 2, 'S'); art_rect(192, 34, 2, 28, 'x');
    art_rect(254, 34, 2, 28, 'C');
    art_rect(210, 44, 12, 18, 'T'); art_rect(211, 45, 10, 17, 't');
    art_rect(218, 54, 2, 2, 'y');
    art_rect(197, 42, 10, 9, 'T'); art_rect(198, 43, 8, 7, 'W');
    art_rect(202, 43, 1, 7, 'T'); art_rect(198, 46, 8, 1, 'T');
    art_rect(237, 42, 10, 9, 'T'); art_rect(238, 43, 8, 7, 'W');
    art_rect(242, 43, 1, 7, 'T'); art_rect(238, 46, 8, 1, 'T');
}

static void draw_static_world(void)
{
    u32 *buf = target;
    u32 cor = pal('g');
    if (estacao == 1) cor = pal('2');
    else if (estacao == 2) cor = pal('3');
    else if (estacao == 3) cor = pal('4');
    u32 n = fb_width * fb_height;
    for (u32 i = 0; i < n; i++) buf[i] = cor;

    int cols = fb_width / TILE + 1;
    int rows = fb_height / TILE + 1;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            char t = tile_at(c, r);
            int sx = c * TILE, sy = r * TILE;
            switch (t) {
            case 'p': fill_rect(sx, sy, TILE, TILE, pal('d')); draw_sprite(sx, sy, spr_dirt); break;
            case '.': {
                unsigned h = tile_hash(c, r) % 100;
                if (h < 8) draw_sprite(sx, sy, spr_tuftA);
                else if (h < 16) draw_sprite(sx, sy, spr_tuftB);
                else if (h < 20) draw_sprite(sx, sy, spr_flowerR);
                else if (h < 24) draw_sprite(sx, sy, spr_flowerY);
                break;
            }
            case 'T': shade_rect(sx + 1 * S, sy + 8 * S, 14 * S, 6 * S); draw_sprite(sx, sy, spr_trunk); break;
            case 'R': shade_rect(sx + 1 * S, sy + 11 * S, 15 * S, 4 * S); draw_sprite(sx, sy, spr_rock); break;
            case 'O': fill_rect(sx, sy, TILE, TILE, pal('d')); draw_sprite(sx, sy, spr_well); break;
            case 'V': shade_rect(sx + 1 * S, sy + 11 * S, 14 * S, 4 * S); draw_sprite(sx, sy, spr_bush); break;
            default: break;
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
            int sx = c * TILE, sy = r * TILE;
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
            if (tile_at(c, r) == 'T') draw_sprite(c * TILE, (r - 1) * TILE, spr_canopy);
}

static void draw_player(int dir, int anim)
{
    int la = 5, lb = 5;
    if (anim == 1) la = 4;
    if (anim == 2) lb = 4;
    shade_rect(pl_x + 1 * S, pl_y + 18 * S, 10 * S, 2 * S);
    if (dir == 0 || dir == 1) {
        prect(0, 9, 2, 5, 'I'); prect(10, 9, 2, 5, 'I');
        prect(0, 14, 2, 1, 'f'); prect(10, 14, 2, 1, 'f');
        prect(2, 8, 8, 6, 'i'); prect(2, 13, 8, 1, 'I');
        prect(3, 14, 3, la, 'j'); prect(6, 14, 3, lb, 'j');
        prect(3, 14 + la, 3, 1, 't'); prect(6, 14 + lb, 3, 1, 't');
        if (dir == 0) {
            prect(2, 0, 8, 3, 'm'); prect(2, 3, 1, 3, 'm'); prect(9, 3, 1, 3, 'm');
            prect(3, 3, 6, 5, 'f'); prect(4, 5, 1, 1, 'z'); prect(7, 5, 1, 1, 'z');
        } else {
            prect(2, 0, 8, 8, 'm'); prect(3, 6, 6, 2, 'M');
        }
    } else {
        int ax = (anim == 1) ? 3 : (anim == 2) ? 6 : 4;
        int bx = (anim == 1) ? 6 : (anim == 2) ? 3 : 4;
        prect(3, 8, 6, 6, 'i'); prect(3, 13, 6, 1, 'I');
        prect(5, 9, 2, 5, 'I'); prect(5, 14, 2, 1, 'f');
        prect(ax, 14, 3, 5, 'j'); prect(ax, 19, 3, 1, 't');
        prect(bx, 14, 3, 4, 'j'); prect(bx, 18, 3, 1, 't');
        prect(2, 0, 8, 3, 'm');
        if (dir == 3) { prect(2, 3, 4, 4, 'm'); prect(6, 3, 4, 5, 'f'); prect(8, 5, 1, 1, 'z'); }
        else { prect(6, 3, 4, 4, 'm'); prect(2, 3, 4, 5, 'f'); prect(3, 5, 1, 1, 'z'); }
    }
}

/* Desenha um dígito em blocos (fonte 3x5) */
static void draw_digit(int dx, int dy, int num, u32 cor)
{
    static const unsigned char font[10][5] = {
        {0b111,0b101,0b101,0b101,0b111}, /* 0 */
        {0b010,0b110,0b010,0b010,0b111}, /* 1 */
        {0b111,0b001,0b111,0b100,0b111}, /* 2 */
        {0b111,0b001,0b111,0b001,0b111}, /* 3 */
        {0b101,0b101,0b111,0b001,0b001}, /* 4 */
        {0b111,0b100,0b111,0b001,0b111}, /* 5 */
        {0b111,0b100,0b111,0b101,0b111}, /* 6 */
        {0b111,0b001,0b010,0b010,0b010}, /* 7 */
        {0b111,0b101,0b111,0b101,0b111}, /* 8 */
        {0b111,0b101,0b111,0b001,0b111}  /* 9 */
    };
    if (num < 0 || num > 9) return;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (font[num][row] & (1 << (2 - col)))
                fill_rect(dx + col * S, dy + row * S, S, S, cor);
        }
    }
}

/* Relogio no canto superior direito: HH:MM */
static void draw_clock(void)
{
    int h = hora_atual;
    if (h >= 24) h -= 24; /* 24,25,26 -> 0,1,2 */
    int m = (minuto_atual / 10) * 10;

    int base_x = (int)fb_width - 40 * S;
    int base_y = 4 * S;

    /* fundo */
    fill_rect(base_x - 4 * S, base_y - 2 * S, 38 * S, 12 * S, pal('T'));
    fill_rect(base_x - 2 * S, base_y, 34 * S, 8 * S, pal('c'));

    u32 cor = pal('z');

    /* hora (2 digitos) */
    int h1 = h / 10;
    int h2 = h % 10;
    if (h1 > 0) draw_digit(base_x, base_y, h1, cor);
    draw_digit(base_x + (h1 > 0 ? 4 : 0) * S, base_y, h2, cor);

    /* dois pontos */
    int colon_x = base_x + (h1 > 0 ? 8 : 4) * S;
    fill_rect(colon_x, base_y + 1 * S, S, S, cor);
    fill_rect(colon_x, base_y + 3 * S, S, S, cor);

    /* minutos (2 digitos) */
    int m1 = m / 10;
    int m2 = m % 10;
    draw_digit(colon_x + 2 * S, base_y, m1, cor);
    draw_digit(colon_x + 6 * S, base_y, m2, cor);
}

/* HUD esquerda: energia, agua, dia, estacao */
static void draw_hud(void)
{
    art_rect(1, 1, 50, 20, 'T');
    art_rect(2, 2, 48, 18, 'c');

    /* Energia */
    int e = energia * 44 / ENERGY_MAX;
    art_rect(3, 10, 44, 2, 'S');
    art_rect(3, 10, e, 2, (energia < 20) ? 'i' : 'h');

    /* Agua */
    int w = agua_regador * 44 / WATER_MAX;
    art_rect(3, 13, 44, 1, 'S');
    art_rect(3, 13, w, 1, 'v');

    /* Moedas */
    int m = moedas / 5; if (m > 8) m = 8;
    for (int i = 0; i < m; i++) art_rect(3 + i * 5, 3, 4, 3, 'y');

    /* Colheita */
    int n = colheita; if (n > 8) n = 8;
    for (int i = 0; i < n; i++) art_rect(3 + i * 5, 7, 3, 2, 'r');

    /* Estacao (4 blocos) */
    char est = (estacao == 0) ? 'h' : (estacao == 1) ? 'y' : (estacao == 2) ? 'o' : 'x';
    for (int i = 0; i < 4; i++) {
        if (i == estacao) art_rect(44 + i * 2, 3, 1, 1, est);
        else art_rect(44 + i * 2, 3, 1, 1, 'S');
    }
}

static void draw_flash(void)
{
    if (flash_timer > 0) {
        int a = flash_timer * 12;
        if (a > 200) a = 200;
        fill_rect(0, 0, fb_width, a, 0x00FFFFFF);
        flash_timer--;
    }
}

static void draw_rain(void)
{
    if (!chovendo) return;
    int t = chuva_timer;
    for (int i = 0; i < 80; i++) {
        int x = (i * 137 + t * 5) % (int)fb_width;
        int y = (i * 89 + t * 11) % (int)fb_height;
        fill_rect(x, y, S, S * 2, 0x00A0C0F0);
    }
}

static int blocked_tile(char t)
{
    return t == 'T' || t == 'R' || t == 'H' || t == 'w' || t == 'O' || t == 'V';
}

static int colide(int px, int py)
{
    int x0 = px + 2 * S, x1 = px + 10 * S - 1;
    int y0 = py + 16 * S, y1 = py + 20 * S - 1;
    if (x0 < 0 || y0 < 0) return 0;
    return blocked_tile(tile_at(x0 / TILE, y0 / TILE)) ||
           blocked_tile(tile_at(x1 / TILE, y0 / TILE)) ||
           blocked_tile(tile_at(x0 / TILE, y1 / TILE)) ||
           blocked_tile(tile_at(x1 / TILE, y1 / TILE));
}

static int perto_da_casa(int px, int py)
{
    int c = (px + 6 * S) / TILE, r = (py + 18 * S) / TILE;
    return tile_at(c, r) == 'H' || tile_at(c, r + 1) == 'H' ||
           tile_at(c - 1, r) == 'H' || tile_at(c + 1, r) == 'H';
}

static int perto_do_poco(int px, int py)
{
    int c = (px + 6 * S) / TILE, r = (py + 18 * S) / TILE;
    return tile_at(c, r) == 'O' || tile_at(c + 1, r) == 'O' ||
           tile_at(c - 1, r) == 'O' || tile_at(c, r - 1) == 'O' ||
           tile_at(c, r + 1) == 'O';
}

static int perto_do_lago(int px, int py)
{
    int c = (px + 6 * S) / TILE, r = (py + 18 * S) / TILE;
    return tile_at(c, r) == 'w' || tile_at(c + 1, r) == 'w' ||
           tile_at(c - 1, r) == 'w' || tile_at(c, r - 1) == 'w' ||
           tile_at(c, r + 1) == 'w';
}

static void acao_fazenda(int px, int py, int cross, int square, int circle)
{
    int col = (px + 6 * S) / TILE - FARM_C0;
    int row = (py + 18 * S) / TILE - FARM_R0;

    if (circle && (perto_do_poco(px, py) || perto_do_lago(px, py))) {
        agua_regador = WATER_MAX;
        flash_timer = 6;
        return;
    }

    if (col < 0 || col >= FARM_W || row < 0 || row >= FARM_H) return;
    unsigned char *st = &farm_state[row][col];

    if (cross) {
        if (*st == 0 && energia >= 2) { *st = 1; energia -= 2; flash_timer = 4; }
        else if (*st == 1 && energia >= 2) { *st = 2; energia -= 2; flash_timer = 4; }
        else if (*st == 4) { *st = 1; farm_water[row][col] = 0; colheita++; flash_timer = 6; }
    }

    if (square && agua_regador > 0 && !farm_water[row][col] && energia >= 1) {
        if (*st >= 1 && *st <= 3) {
            int dx = 0, dy = 0;
            if (can_level == 1) { dx = 0; dy = 0; }
            else if (can_level == 2) { dx = 1; dy = 0; }
            else if (can_level == 3) { dx = 1; dy = 1; }
            for (int rr = row - (can_level == 3 ? 1 : 0); rr <= row + (can_level == 3 ? 1 : 0); rr++) {
                for (int cc = col - (can_level == 3 ? 1 : 0); cc <= col + dx; cc++) {
                    if (rr >= 0 && rr < FARM_H && cc >= 0 && cc < FARM_W) {
                        if (farm_state[rr][cc] >= 1 && farm_state[rr][cc] <= 3 && !farm_water[rr][cc]) {
                            farm_water[rr][cc] = 1;
                            agua_regador--;
                            energia -= 1;
                            if (agua_regador <= 0) break;
                        }
                    }
                }
                if (agua_regador <= 0) break;
            }
            flash_timer = 4;
        }
    }
}

static void vender_tudo(void)
{
    if (colheita <= 0) return;
    int ganho = colheita * 15;
    moedas += ganho;
    colheita = 0;
    flash_timer = 10;
}

/* Atualiza hora/minuto e escurecimento */
static void atualiza_tempo(void)
{
    tempo_frames++;

    /* 6:00 = frame 0. Cada FRAMES_PER_10MIN avanca 10 min */
    int total_min = (tempo_frames / FRAMES_PER_10MIN) * 10;
    int h = 6 + total_min / 60;
    int m = total_min % 60;

    if (h >= 26) {
        /* 2:00 - desmaio */
        h = 26; m = 0;
        if (!desmaiou) {
            desmaiou = 1;
            /* perde 10% das moedas, max 1000 */
            int perda = moedas / 10;
            if (perda > 1000) perda = 1000;
            moedas -= perda;
            energia = ENERGY_MAX / 2;
            flash_timer = 30;
        }
    }

    hora_atual = h;
    minuto_atual = m;

    /* Escurecimento baseado na estacao */
    /* Primavera/Verao: escurece as 20:00. Outono: 19:00. Inverno: 18:00 */
    int hora_escurecer = (estacao == 0 || estacao == 1) ? 20 : (estacao == 2) ? 19 : 18;
    if (h >= hora_escurecer) {
        int horas_escuras = h - hora_escurecer;
        escuro = horas_escuras * 15; /* cada hora escurece 15% */
        if (escuro > 75) escuro = 75;
    } else {
        escuro = 0;
    }
}

static void dormir(void)
{
    if (!perto_da_casa(pl_x, pl_y)) return;

    /* Calcula energia recuperada baseada na hora */
    /* Antes da meia-noite: 100%. Depois: proporcional */
    int h = hora_atual;
    if (h >= 26) h = 26;
    if (h <= 24) {
        energia = ENERGY_MAX;
    } else {
        /* 24:00 a 26:00 -> 100% a 50% */
        int frac = (h - 24) * 50 / 2; /* 0 a 50 */
        energia = ENERGY_MAX * (100 - frac) / 100;
    }
    exausto = 0;
    desmaiou = 0;

    /* Avanca dia */
    dia++;
    tempo_frames = 0;
    hora_atual = 6;
    minuto_atual = 0;

    /* Fazenda cresce */
    for (int r = 0; r < FARM_H; r++)
        for (int c = 0; c < FARM_W; c++) {
            if ((farm_state[r][c] == 2 || farm_state[r][c] == 3) && farm_water[r][c])
                farm_state[r][c]++;
            farm_water[r][c] = 0;
        }

    /* Muda estacao a cada 5 dias */
    if (dia % 5 == 0) {
        estacao = (estacao + 1) % 4;
        if (cache) { target = cache; draw_static_world(); }
    }
    flash_timer = 12;
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
    for (int i = 0; i < 2; i++) rsxFree(fb_addr[i]);
    rsxFinish(context, 1);
    free(host_addr);
    if (cache) free(cache);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    padInfo padinfo; padData paddata;
    int px, py; int dir = 0; int anim_t = 0; int frame_count = 0;
    int prev_cross = 0, prev_square = 0, prev_circle = 0;
    int prev_l1 = 0, prev_r1 = 0, prev_start = 0, prev_r2 = 0;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_video();

    S = fb_height / 180; if (S < 2) S = 2;
    px = 6 * TILE; py = 4 * TILE;

    cache = (u32 *)memalign(128, fb_width * fb_height * 4);
    if (cache) { target = cache; draw_static_world(); }
    for (int i = 0; i < 2; i++) {
        target = fb_addr[i];
        if (cache) memcpy(fb_addr[i], cache, fb_width * fb_height * 4);
        else draw_static_world();
    }

    curr_fb = 0;
    gcmResetFlipStatus();
    gcmSetFlip(context, 1);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    curr_fb = 0;

    while (running) {
        sysUtilCheckCallback();
        if (menu_aberto) { usleep(20000); continue; }

        int mx = 0, my = 0;
        int cross = 0, square = 0, circle = 0;
        int l1 = 0, r1 = 0, start = 0, r2 = 0;
        int sp = S * 3 / 2;

        ioPadGetInfo(&padinfo);
        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);
            int c_now = paddata.BTN_CROSS, s_now = paddata.BTN_SQUARE;
            int o_now = paddata.BTN_CIRCLE, l1_now = paddata.BTN_L1;
            int r1_now = paddata.BTN_R1, st_now = paddata.BTN_START;
            int r2_now = paddata.BTN_R2;
            cross = c_now && !prev_cross; square = s_now && !prev_square;
            circle = o_now && !prev_circle; l1 = l1_now && !prev_l1;
            r1 = r1_now && !prev_r1; start = st_now && !prev_start;
            r2 = r2_now && !prev_r2;
            prev_cross = c_now; prev_square = s_now; prev_circle = o_now;
            prev_l1 = l1_now; prev_r1 = r1_now; prev_start = st_now; prev_r2 = r2_now;
            if (start) running = 0;

            int dx = (int)paddata.ANA_L_H - 128;
            int dy = (int)paddata.ANA_L_V - 128;
            if (dx > 30 || dx < -30) mx += dx * S / 64;
            if (dy > 30 || dy < -30) my += dy * S / 64;
            if (paddata.BTN_LEFT) mx -= sp; if (paddata.BTN_RIGHT) mx += sp;
            if (paddata.BTN_UP) my -= sp; if (paddata.BTN_DOWN) my += sp;
        }

        int moving = (mx != 0 || my != 0);
        if (moving) {
            if (abs(mx) > abs(my)) dir = (mx < 0) ? 2 : 3;
            else dir = (my < 0) ? 1 : 0;
            anim_t++;
        } else anim_t = 0;
        int seq = (anim_t / 6) & 3;
        int anim = (seq == 1) ? 1 : (seq == 3) ? 2 : 0;

        if (exausto) { mx /= 2; my /= 2; }

        if (!colide(px + mx, py)) px += mx;
        if (!colide(px, py + my)) py += my;

        int pw = 12 * S, ph = 20 * S;
        if (px < 0) px = 0; if (py < 0) py = 0;
        if (px > (int)fb_width - pw) px = fb_width - pw;
        if (py > (int)fb_height - ph) py = fb_height - ph;

        if (r2 && perto_da_casa(px, py)) {
            if (moedas >= 100) {
                moedas -= 100;
                if (hoe_level < 3) hoe_level++;
                else if (can_level < 3) can_level++;
                flash_timer = 8;
            }
        }

        acao_fazenda(px, py, cross, square, circle);
        if (l1) vender_tudo();
        if (r1) dormir();

        /* Avanca tempo */
        if (!desmaiou) atualiza_tempo();

        /* Se desmaiou, forca dormir */
        if (desmaiou) {
            dormir();
            px = 6 * TILE; py = 4 * TILE;
        }

        chuva_timer++;
        if (!chovendo && chuva_timer > 1800) {
            chovendo = 1; chuva_timer = 0;
            for (int r = 0; r < FARM_H; r++)
                for (int c = 0; c < FARM_W; c++)
                    if (farm_state[r][c] >= 1 && farm_state[r][c] <= 3) farm_water[r][c] = 1;
        } else if (chovendo && chuva_timer > 600) {
            chovendo = 0; chuva_timer = 0;
        }

        if (energia <= 0) exausto = 1;
        if (energia < -15) energia = -15;

        frame_count++;
        wait_flip();

        target = fb_addr[curr_fb];
        if (cache) memcpy(target, cache, fb_width * fb_height * 4);
        else draw_static_world();

        draw_dynamic(frame_count);
        pl_x = px; pl_y = py;
        draw_player(dir, anim);
        draw_canopies();
        draw_rain();

        /* Escurece conforme hora */
        if (escuro > 0) darken_screen(escuro);

        draw_hud();
        draw_clock();
        draw_flash();
        flip_screen();
    }

    shutdown_video();
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    return 0;
}
