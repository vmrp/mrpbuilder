
#include "../../header/bridge.h"

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
mr_internal_table *mr;

int32 mrc_init(void) {
    mrc_clearScreen(0, 0, 0);

    // 判断是不是精简版mythroad
    if (mr_table->_mr_c_internal_table->mrp_gettop) {
        mrc_drawText("mr_vm_full", 0, 30, 255, 255, 255, 0, 1);
        mr = mr_table->_mr_c_internal_table;
    } else {
        mrc_drawText("mr_vm_mini", 0, 30, 255, 255, 255, 0, 1);
    }

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

#define MRP_GLOBALSINDEX (-10001)
#define MRP_TFUNCTION 6

int32 mrc_event(int32 code, int32 p0, int32 p1) {
    if (code == MR_MOUSE_DOWN || code == MR_MOUSE_MOVE) {
        if (mr) {
            int32 vm_state = (int32)*mr->vm_state;
            mr->mrp_pushstring(vm_state, "extCallMr");
            mr->mrp_gettable(vm_state, MRP_GLOBALSINDEX);
            if (mr->mrp_type(vm_state, -1) == MRP_TFUNCTION) {
                char str[50];
                mr_table->sprintf(str, "from ext x=%d,y=%d", p0, p1);

                mr->mrp_pushstring(vm_state, str);
                mr->mrp_pcall(vm_state, 1, 0, 0);
            } else {
                mr->mrp_settop(vm_state, -2);
            }
        }
    } else if (code == MR_KEY_RELEASE) {
        if (p0 == MR_KEY_SELECT) {
        }
        inFuncs->mrc_exit();
    }
    return 0;
}
