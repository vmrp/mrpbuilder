#include "mr_helper.h"

#define C_FUNCTION_P() (*(((mr_c_function_st **)mr_c_function_load) - 1))
#define HELPER() (*(((mr_table **)mr_c_function_load) - 2))

extern unsigned int mr_helper_get_rw_len(void);
extern unsigned int mr_helper_get_rw_lenOnly(void);
extern unsigned int mr_helper_get_ro_len(void);
extern void mr_helper_set_r9(void *data);

int32 mrc_init(void);
int32 mr_c_function_load(int32 code);
void mrc_timerTimeout(void);
int32 mrc_event(int32 code, int32 param0, int32 param1);
int32 mrc_pause(void);
int32 mrc_resume(void);
int32 mrc_exitApp(void);

T__DrawText mrc_drawText;
T_DrawRect mrc_drawRect;

void *mrc_malloc(uint32 len) {
    uint32 *p = HELPER()->mr_malloc(len + sizeof(uint32));
    if (p) {
        *p = len;
        return p + 1;
    }
    return p;
}

int32 mr_helper(mr_c_function_st *P, int32 code, uint8 *input, int32 input_len, uint8 **output, int32 *output_len) {
    int32 ret = 0;
#ifdef __arm
    register void *oldR9;
    __asm { mov oldR9, r9 }
#endif
    mr_helper_set_r9(P->start_of_ER_RW);

    switch (code) {
        case 0: {
            mr_table *helper = HELPER();
            mrc_drawText = helper->_DrawText;
            mrc_drawRect = helper->DrawRect;
            helper->_mr_TestCom(0, 7, 9999);
            ret = mrc_init();
            *(helper->_mr_c_internal_table->mr_timer_p) = "dealtimer";
            break;
        }
        case 1: {
            mr_c_event_st *e = (mr_c_event_st *)input;
            ret = mrc_event(e->code, e->param0, e->param1);
            if (e->code == 8) {
                ret = mrc_exitApp();
            }
            break;
        }
        case 2:
            mrc_timerTimeout();
            break;
        case 4:
            mrc_pause();
            break;
        case 5:
            mrc_resume();
            break;
        case 9: {
            uint32 *ptr = (uint32 *)input;
            if (ptr[0] != 0) {
                typedef int32 (*Func)(uint32 p0, uint32 p1, uint32 p2, uint32 p3, uint32 p4, uint32 p5);
                Func func = (Func)ptr[0];
                ret = func(ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6]);
            }
        }
    }
#ifdef __arm
    __asm { mov r9, oldR9 }
#endif
    return ret;
}

int32 mr_c_function_load(int32 code) {
    mr_table *helper = HELPER();

    if (helper->_mr_c_function_new(mr_helper, sizeof(mr_c_function_st)) != -1) {
        mr_c_function_st *P = C_FUNCTION_P();  // 注意_mr_c_function_new()成功之后才能获取值
        P->ext_type = 0;
        P->ER_RW_Length = mr_helper_get_rw_len();
        P->start_of_ER_RW = mrc_malloc(P->ER_RW_Length);
        if (P->start_of_ER_RW) {
            uint32 rwLenOnly = mr_helper_get_rw_lenOnly();
            helper->memcpy(P->start_of_ER_RW,
                           mr_helper_get_ro_len() + (char *)mr_c_function_load,
                           rwLenOnly);

            helper->memset(P->start_of_ER_RW + rwLenOnly, 0, P->ER_RW_Length - rwLenOnly);
            return 0;
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////
void mrc_timerTimeout(void) {
}

int32 mrc_init(void) {
    mr_table *helper = HELPER();
    mrc_drawRect(0, 0, 240, 320, 0, 0, 0);
    mrc_drawText("hello a.c", 0, 0, 255, 255, 0, 0, 1);
    helper->mr_drawBitmap(*(helper->mr_screenBuf), 0, 0, *(helper->mr_screen_w), *(helper->mr_screen_h));
    return 0;
}

enum {
    MR_KEY_PRESS,      /*0*/
    MR_KEY_RELEASE,    /*1*/
    MR_MOUSE_DOWN,     /*2*/
    MR_MOUSE_UP,       /*3*/
    MR_MENU_SELECT,    /*4*/
    MR_MENU_RETURN,    /*5*/
    MR_DIALOG_EVENT,   /*6*/
    MR_SMS_INDICATION, /*7*/
    MR_EVENT_EXIT,     /*8*/
    MR_EXIT_EVENT = 8, /*8*/
    MR_SMS_RESULT,     /*9*/
    MR_LOCALUI_EVENT,  /*10*/
    MR_OSD_EVENT,      /*11*/
    MR_MOUSE_MOVE,     /*12*/
    MR_ERROR_EVENT,    /*13执行异常通过这个事件来通知*/
    MR_PHB_EVENT,
    MR_SMS_OP_EVENT,
    MR_SMS_GET_SC,
    MR_DATA_ACCOUNT_EVENT,
    MR_MOTION_EVENT
};

int32 mrc_event(int32 code, int32 p0, int32 p1) {
    if (code == MR_MOUSE_DOWN || code == MR_MOUSE_MOVE) {
        char text[40];
        mr_table *helper = HELPER();

        int32 SCRW = *(helper->mr_screen_w);
        int32 SCRH = *(helper->mr_screen_h);
        uint16 *scrBuf = *(helper->mr_screenBuf);

        mrc_drawRect(0, 0, SCRW, SCRH, 0xff, 0xff, 0xff);
        mrc_drawRect(p0, 0, 1, SCRH, 0xff, 0, 0);
        mrc_drawRect(0, p1, SCRW, 1, 0xff, 0, 0);
        helper->sprintf(text, "x=%d,y=%d", p0, p1);
        mrc_drawText(text, 0, 0, 255, 0, 0, 0, 1);
        helper->mr_drawBitmap(scrBuf, 0, 0, SCRW, SCRH);
    }
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