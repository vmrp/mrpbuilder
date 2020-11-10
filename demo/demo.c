
#include "../header/bridge.h"

int32 mrc_init(void);
int32 mrc_pause(void);
int32 mrc_resume(void);
int32 mrc_exitApp(void);
int32 mrc_event(int32 code, int32 p0, int32 p1);

uint16 SCRW, SCRH;
mr_table_st *mr_table;
mr_c_function_st *mr_c_function;
inFuncs_st *inFuncs;
void *(*mrc_malloc)(int size);
void (*mrc_free)(void *address);
void (*mrc_clearScreen)(int32 r, int32 g, int32 b);
int32 (*mrc_drawText)(char *pcText, int16 x, int16 y, uint8 r, uint8 g, uint8 b, int is_unicode, uint16 font);
void (*mrc_refreshScreen)(int16 x, int16 y, uint16 w, uint16 h);

outFuncs_st *_start(inFuncs_st *in) {
    outFuncs_st *ret = in->mrc_malloc(sizeof(outFuncs_st));
    ret->mrc_init = mrc_init;
    ret->mrc_event = mrc_event;
    ret->mrc_exitApp = mrc_exitApp;
    ret->mrc_pause = mrc_pause;
    ret->mrc_resume = mrc_resume;

    // 把一些常用的函数放到全局变量
    mr_table = in->mr_table;
    mr_c_function = in->mr_c_function;
    mrc_malloc = in->mrc_malloc;
    mrc_free = in->mrc_free;
    mrc_clearScreen = in->mrc_clearScreen;
    mrc_drawText = in->mrc_drawText;
    mrc_refreshScreen = in->mrc_refreshScreen;

    SCRW = (uint16)(*in->mr_table->mr_screen_w);
    SCRH = (uint16)(*in->mr_table->mr_screen_h);

    inFuncs = in;
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////

#define MAKERGB(r, g, b) (uint16)(((uint32)(r >> 3) << 11) | ((uint32)(g >> 2) << 5) | ((uint32)(b >> 3)))
#define BMPW 16
#define BMPH 16

void testBmp() {
    char *bmp;
    int32 bmpLen;
    bmp = inFuncs->mrc_readFileFromMrp("test.bmp", &bmpLen, 0);
    if (bmp) {
        mr_table->_DrawBitmap((uint16 *)bmp, 0, 0, BMPW, BMPH, BM_TRANSPARENT, MAKERGB(0xff, 0x00, 0xff), 0, 0, BMPW);
        inFuncs->mrc_freeFileData(bmp, bmpLen);
    }
}

int32 mrc_init(void) {
    mrc_clearScreen(0, 0, 0);
    mrc_drawText("sample.c event test", 0, 30, 255, 255, 255, 0, 1);
    testBmp();
    mrc_refreshScreen(0, 0, SCRW, SCRH);
    return 0;
}

int32 mrc_pause(void) {
    return 0;
}

int32 mrc_resume(void) {
    return 0;
}

int32 mrc_exitApp(void) {
    return 0;
}

int32 mrc_event(int32 code, int32 p0, int32 p1) {
    if (code == MR_MOUSE_DOWN || code == MR_MOUSE_MOVE) {
        char text[40];

        mrc_clearScreen(0xff, 0xff, 0xff);
        mr_table->DrawRect(p0, 0, 1, SCRH, 0xff, 0, 0);
        mr_table->DrawRect(0, p1, SCRW, 1, 0xff, 0, 0);
        mr_table->sprintf(text, "x=%d,y=%d", p0, p1);
        mrc_drawText(text, 0, 0, 255, 0, 0, 0, 1);
        mrc_refreshScreen(0, 0, SCRW, SCRH);

    } else if (code == MR_KEY_RELEASE) {
        if (p0 == MR_KEY_SELECT) {
            inFuncs->mrc_exit();
        }
    }
    return 0;
}
