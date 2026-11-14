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

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

int main(int argc, char *argv[])
{
    padData paddata;

    void *host_addr = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);

    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    ioPadInit(7);

    while (running) {
        sysUtilCheckCallback();
        ioPadGetData(0, &paddata);
        if (paddata.BTN_START)
            running = 0;
        usleep(16000);
    }

    ioPadEnd();
    return 0;
}
