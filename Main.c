#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/process.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#define GCM_LABEL_INDEX 255

static gcmContextData *context;
static u32 cur_buffer = 0;
static u32 *buffer[2];
static u32 offset[2];
static u32 width, height, pitch;
static volatile int running = 1;

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

static void init_screen(void)
{
    void *host = memalign(1024 * 1024, 1024 * 1024);
    context = rsxInit(0x10000, 1024 * 1024, host);

    videoState state;
    videoGetState(0, 0, &state);

    videoResolution res;
    videoGetResolution(state.displayMode.resolution, &res);
    width = res.width;
    height = res.height;
    pitch = width * 4;

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = pitch;
    videoConfigure(0, &vconfig, NULL, 0);

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < 2; i++) {
        buffer[i] = (u32 *)rsxMemalign(64, height * pitch);
        rsxAddressToOffset(buffer[i], &offset[i]);
        gcmSetDisplayBuffer(i, offset[i], pitch, width, height);
    }
}

static void draw_frame(int x, int y)
{
    u32 *buf = buffer[cur_buffer];

    for (u32 i = 0; i < width * height; i++)
        buf[i] = 0x00336699;

    for (int j = 0; j < 50; j++)
        for (int i = 0; i < 50; i++)
            if (x + i >= 0 && x + i < (int)width && y + j >= 0 && y + j < (int)height)
                buf[(y + j) * width + (x + i)] = 0x0000FF00;
}

int main(int argc, char *argv[])
{
    padInfo padinfo;
    padData paddata;
    int x = 100, y = 100;

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);
    init_screen();

    while (running) {
        sysUtilCheckCallback();
        ioPadGetInfo(&padinfo);

        if (padinfo.status[0]) {
            ioPadGetData(0, &paddata);
            if (paddata.BTN_LEFT)  x -= 4;
            if (paddata.BTN_RIGHT) x += 4;
            if (paddata.BTN_UP)    y -= 4;
            if (paddata.BTN_DOWN)  y += 4;
        }

        draw_frame(x, y);
        gcmResetFlipStatus();
        gcmSetFlip(context, cur_buffer);
        rsxFlushBuffer(context);
        gcmSetWaitFlip(context);
        cur_buffer = !cur_buffer;
        usleep(16000);
    }

    ioPadEnd();
    return 0;
}
