#include "mrc_base.h"

int32 mrc_init(void) {
    mrc_clearScreen(0, 0, 0);
    mrc_drawText("helloworld", 0, 0, 255, 255, 255, 0, 1);
    mrc_refreshScreen(0, 0, 240, 320);
    return 0;
}

int32 mrc_exitApp(void) {
    return 0;
}

int32 mrc_event(int32 code, int32 param0, int32 param1) {
    return 0;
}

int32 mrc_pause() {
    return 0;
}

int32 mrc_resume() {
    return 0;
}

int32 mrc_extRecvAppEventEx(int32 code, int32 param0, int32 param1) {
    return MR_SUCCESS;
}

int32 mrc_extRecvAppEvent(int32 app, int32 code, int32 param0, int32 param1) {
    return MR_SUCCESS;
}