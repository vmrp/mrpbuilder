
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

#define BLOCK_SIZE 4
#define TABLE_V(x, y) (*(table + (x) + (y)*tableW))
#define NEXT_TABLE_V(x, y) (*(nextTable + (x) + (y)*tableW))
int tableW, tableH;
uint8 *table, *nextTable;
int32 timer, tableLen;

void drawBlock(int x, int y, uint8 color, int update) {
    x = x * (BLOCK_SIZE + 1);
    y = y * (BLOCK_SIZE + 1);

    mr_table->DrawRect(x, y, BLOCK_SIZE, BLOCK_SIZE, color, color, color);
    if (update) {
        mrc_refreshScreen(x, y, BLOCK_SIZE, BLOCK_SIZE);
    }
}

void initTable() {
    tableW = (SCRW + 1) / (BLOCK_SIZE + 1);
    tableH = (SCRH + 1) / (BLOCK_SIZE + 1);
    tableLen = tableW * tableH;
    table = mrc_malloc(tableLen);
    nextTable = mrc_malloc(tableLen);

    mr_table->memset(table, 0, tableLen);
    mr_table->memset(nextTable, 0, tableLen);

    // TABLE_V(0, 0) = 0;
    // TABLE_V(2, 0) = 0;
    // TABLE_V(0, 2) = 0;
    // TABLE_V(tableW - 1, tableH - 1) = 0;
    inFuncs->mrc_sand(mr_table->mr_getTime());

    for (int y = 0; y < tableH; y++) {
        for (int x = 0; x < tableW; x++) {
            // if (mr_table->rand() < 9000) {
            if (inFuncs->mrc_rand() < 9000) {
                TABLE_V(x, y) = 1;
            }
            uint8 color = TABLE_V(x, y) ? 0x00 : 0xff;
            drawBlock(x, y, color, 0);
        }
    }
}

void changeTable(int x, int y) {
    // 如果余数为0，说明点在框上
    if ((x % (BLOCK_SIZE + 1)) && (y % (BLOCK_SIZE + 1))) {
        x = x / (BLOCK_SIZE + 1);
        y = y / (BLOCK_SIZE + 1);
        uint8 v = TABLE_V(x, y) ? 0 : 1;
        TABLE_V(x, y) = v;
        drawBlock(x, y, v ? 0x00 : 0xff, 1);
    }
}

void loop(int32 data) {
    mr_table->memcpy(nextTable, table, tableLen);

    for (int y = 0; y < tableH; y++) {
        for (int x = 0; x < tableW; x++) {
            uint8 currentState = TABLE_V(x, y);
            int num = 0;

            if (x > 0) {
                if (y > 0) num += TABLE_V(x - 1, y - 1);           // 左上
                num += TABLE_V(x - 1, y);                          // 左
                if (y + 1 < tableH) num += TABLE_V(x - 1, y + 1);  // 左下
            }
            if (y > 0) num += TABLE_V(x, y - 1);           // 上
            if (y + 1 < tableH) num += TABLE_V(x, y + 1);  // 下
            if (x + 1 < tableW) {
                if (y > 0) num += TABLE_V(x + 1, y - 1);           // 右上
                num += TABLE_V(x + 1, y);                          // 右
                if (y + 1 < tableH) num += TABLE_V(x + 1, y + 1);  // 右下
            }
            if (currentState) {  // a. 活元胞（黑格）周围的细胞数如果小于2个或多于3个则会死亡；（离群或过度竞争导致死亡）
                if (num < 2 || num > 3) {
                    NEXT_TABLE_V(x, y) = 0;
                    drawBlock(x, y, 0xff, 1);
                } else {
                    // b. 活元胞（黑格）周围如果有2或3个细胞可以继续存活；（正常生存）
                }
            } else if (num == 3) {  // c. 死细胞（白格）周围如果恰好有3个细胞则会诞生新的活细胞。（繁殖）
                NEXT_TABLE_V(x, y) = 1;
                drawBlock(x, y, 0x00, 1);
            }
        }
    }
    uint8 *tmp = table;
    table = nextTable;
    nextTable = tmp;
}

/*
康威的生命游戏 Conway's Game of Life
生命游戏是一个零玩家参与的游戏，由英国数学家约翰·康威（John Conway）在1970年发明的元胞自动机，刊登在当年的《科学美国人》上。
在一个无数格子的网格（元胞）中，每个元胞有两种状态：死（白格）或活（黑格），而下一回合的状态完全受它周围8个元胞的状态而定。按照以下三条规则进行演化：
a. 活元胞（黑格）周围的细胞数如果小于2个或多于3个则会死亡；（离群或过度竞争导致死亡）
b. 活元胞（黑格）周围如果有2或3个细胞可以继续存活；（正常生存）
c. 死细胞（白格）周围如果恰好有3个细胞则会诞生新的活细胞。（繁殖）
*/
int32 mrc_init(void) {
    mrc_clearScreen(0, 0, 0);
    initTable();
    mrc_refreshScreen(0, 0, SCRW, SCRH);
    timer = inFuncs->mrc_timerCreate();
    inFuncs->mrc_timerStart(timer, 100, 0, loop, 1);
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
        changeTable(p0, p1);
    } else if (code == MR_KEY_RELEASE) {
        switch (p0) {
            case MR_KEY_SOFTRIGHT:
                break;
            case MR_KEY_SELECT:
                loop(0);
                break;
            default:
                inFuncs->mrc_exit();
                break;
        }
    }
    return 0;
}
