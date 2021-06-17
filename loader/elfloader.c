#include "elfload.h"
#include "../header/bridge.h"

extern uint32 mrc_rand(void);
extern void mrc_sand(uint32 seed);
extern int32 mr_c_function_load(int32 code);

#define C_FUNCTION_P() (*(((void**)mr_c_function_load) - 1))
#define GET_MR_TABLE() (*(((void**)mr_c_function_load) - 2))

typedef void* (*T_mr_malloc)(uint32 len);
typedef void (*T_mr_free)(void* p, uint32 len);
static T_mr_malloc mr_malloc;
static T_mr_free mr_free;

void logPrint(char* msg) {
    int32 ff = mrc_open("elfloader.log", MR_FILE_CREATE | MR_FILE_WRONLY);
    mrc_seek(ff, 0, MR_SEEK_END);
    mrc_write(ff, msg, mrc_strlen(msg));
    mrc_close(ff);
}

void showText(char* str) {
    mr_screeninfo info;
    mrc_getScreenInfo(&info);
    mrc_clearScreen(0, 0, 0);
    mrc_drawText(str, 0, 0, 255, 255, 255, 0, 1);
    mrc_refreshScreen(0, 0, info.width, info.height);
}

void* rawElf;
int32 rawElfLen;

static BOOL fpread(el_ctx* ctx, void* dest, size_t nb, size_t offset) {
    uint8* p = rawElf;
    p += offset;
    memcpy(dest, p, nb);
    return TRUE;
}

static void* alloccb(el_ctx* ctx, Elf_Addr phys, Elf_Addr virt, Elf_Addr size) {
    // return (void*)virt;
    return (void*)phys;
}

inFuncs_st inFuncs;
outFuncs_st* outFuncs;
void* elfBuf;
void* realElfBuf;
int elfBufLen;

// 实际上由于这个elfloader的功能太简单，加载的代码块有很大一部分空间是浪费掉的，而为了内存对齐又浪费掉一大块内存
static void* allocMem(uint32 align, uint32 memsz) {
    uint32 pp, t;
    // 不能使用mrc_malloc();因为它实际申请的内度长度与传入的长度不同
    // elfBuf = mrc_malloc(ctx.memsz);

    // 获取底层的内存管理函数
    mr_malloc = (T_mr_malloc)(((void**)inFuncs.mr_table)[0]);
    mr_free = (T_mr_free)(((void**)inFuncs.mr_table)[1]);

    elfBufLen = memsz;
    elfBuf = mr_malloc(elfBufLen);
    EL_DEBUG("alloc mem @%p\n", elfBuf);

    pp = (uintptr_t)elfBuf;
    t = pp % align;
    if (t != 0) {  // 内存地址未对齐，采用先申请一块再释放的方法尝试对齐
        void* tmp;
        mr_free(elfBuf, elfBufLen);

        tmp = mr_malloc(t);
        elfBuf = mr_malloc(elfBufLen);
        mr_free(tmp, t);
        EL_DEBUG("mem not align, new1 @%p\n", elfBuf);
    }

    // 再次检查对齐
    pp = (uintptr_t)elfBuf;
    t = pp % align;
    if (t != 0) {  // 仍未对齐，申请足够大的内存
        int n = memsz / align;
        if ((memsz % align) != 0) {
            n++;
        }
        mr_free(elfBuf, elfBufLen);

        elfBufLen = (n + 1) * align;  // n+1是为了确保有足够的空间进行对齐
        realElfBuf = mr_malloc(elfBufLen);

        pp = (uintptr_t)realElfBuf;
        t = align - (pp % align);
        elfBuf = ((char*)realElfBuf + t);

        EL_DEBUG("mem not align, final realElfBuf: @%p, t:%d, elfBufLen:%d, elfBuf:@%p\n", realElfBuf, t, elfBufLen, elfBuf);
    } else {
        realElfBuf = elfBuf;  // realElfBuf用于最终释放
    }
    return elfBuf;
}

int32 mrc_init(void) {
    el_ctx ctx;
    el_status stat;

    showText("gcc mrp v20210614");

    outFuncs = NULL;
    elfBuf = NULL;
    realElfBuf = NULL;

    inFuncs.mr_table = GET_MR_TABLE();
    inFuncs.mr_c_function = C_FUNCTION_P();
    inFuncs.mrc_malloc = mrc_malloc;
    inFuncs.mrc_free = mrc_free;
    inFuncs.mrc_clearScreen = mrc_clearScreen;
    inFuncs.mrc_drawText = mrc_drawText;
    inFuncs.mrc_refreshScreen = mrc_refreshScreen;
    inFuncs.mrc_timerCreate = mrc_timerCreate;
    inFuncs.mrc_timerDelete = mrc_timerDelete;
    inFuncs.mrc_timerStop = mrc_timerStop;
    inFuncs.mrc_timerStart = mrc_timerStart;
    inFuncs.mrc_timerSetTimeEx = mrc_timerSetTimeEx;

    inFuncs.mrc_exit = mrc_exit;
    inFuncs.mrc_readFileFromMrpEx = mrc_readFileFromMrpEx;
    inFuncs.mrc_readFileFromMrpExA = mrc_readFileFromMrpExA;
    inFuncs.mrc_readFileFromMrpExPart = mrc_readFileFromMrpExPart;
    inFuncs.mrc_readFileFromMrp = mrc_readFileFromMrp;
    inFuncs.mrc_freeFileData = mrc_freeFileData;
    inFuncs.mrc_freeOrigin = mrc_freeOrigin;

    inFuncs.mrc_sand = mrc_sand;
    inFuncs.mrc_rand = mrc_rand;
    {
        typedef int32 (*mr_sleep_t)(uint32 ms);
        mr_sleep_t mr_sleep = (mr_sleep_t)(((void**)inFuncs.mr_table)[36]);
        mr_sleep(2000);
    }

    rawElf = mrc_readFileFromMrp("bin.elf", &rawElfLen, 0);
    if (rawElf == NULL) {
        showText("load bin.elf fail.");
        return MR_FAILED;
    }

    ctx.pread = fpread;
    stat = el_init(&ctx);
    if (stat) {
        EL_DEBUG("initialising: error %d\n", stat);
        return MR_FAILED;
    }

    EL_DEBUG("ctx.align:%d, ctx.memsz:%d\n", ctx.align, ctx.memsz);
    elfBuf = allocMem(ctx.align, ctx.memsz);

    ctx.base_load_vaddr = ctx.base_load_paddr = (uintptr_t)elfBuf;
    stat = el_load(&ctx, alloccb);
    if (stat) {
        EL_DEBUG("loading: error %d\n", stat);
        return MR_FAILED;
    }

    stat = el_relocate(&ctx);
    if (stat) {
        EL_DEBUG("relocating: error %d\n", stat);
        return MR_FAILED;
    }
    mrc_freeFileData(rawElf, rawElfLen);
    {
        _START ep = (_START)(ctx.ehdr.e_entry + (uintptr_t)elfBuf);
        EL_DEBUG("Binary entrypoint is 0x%X; invoking 0x%p\n", ctx.ehdr.e_entry, ep);
        outFuncs = ep(&inFuncs);
        EL_DEBUG("invoking entrypoint done. ret: 0x%p\n", outFuncs);
        return outFuncs->mrc_init();
    }
    return 0;
}

int32 mrc_exitApp(void) {
    int32 ret = MR_SUCCESS;
    if (outFuncs) {
        ret = outFuncs->mrc_exitApp();
    }
    if (realElfBuf) {
        mr_free(realElfBuf, elfBufLen);
        realElfBuf = NULL;
        elfBuf = NULL;
    }
    return ret;
}

int32 mrc_event(int32 code, int32 param0, int32 param1) {
    int ret = MR_SUCCESS;
    if (outFuncs) {
        ret = outFuncs->mrc_event(code, param0, param1);
    }
    return ret;
}

int32 mrc_pause() {
    int ret = MR_SUCCESS;
    if (outFuncs) {
        ret = outFuncs->mrc_pause();
    }
    return ret;
}

int32 mrc_resume() {
    int ret = MR_SUCCESS;
    if (outFuncs) {
        ret = outFuncs->mrc_resume();
    }
    return ret;
}

int32 mrc_extRecvAppEventEx(int32 code, int32 param0, int32 param1) {
    return MR_SUCCESS;
}

int32 mrc_extRecvAppEvent(int32 app, int32 code, int32 param0, int32 param1) {
    return MR_SUCCESS;
}