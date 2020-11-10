#include "elfload.h"
#include "../header/bridge.h"

extern int32 mr_c_function_load(int32 code);
#define C_FUNCTION_P() (*(((void**)mr_c_function_load) - 1))
#define GET_MR_TABLE() (*(((void**)mr_c_function_load) - 2))

void logPrint(char* msg) {
    int32 ff = mrc_open("elfloader.log", MR_FILE_CREATE | MR_FILE_WRONLY);
    mrc_seek(ff, 0, MR_SEEK_END);
    mrc_write(ff, msg, mrc_strlen(msg));
    mrc_close(ff);
}

#define R_AARCH64_NONE 0
// #define R_AARCH64_RELATIVE 1027
#define R_AARCH64_RELATIVE 23

el_status el_applyrela(el_ctx* ctx, Elf_RelA* rel) {
    uintptr_t* p = (uintptr_t*)(rel->r_offset + ctx->base_load_paddr);
    uint32_t type = ELF_R_TYPE(rel->r_info);
    uint32_t sym = ELF_R_SYM(rel->r_info);

    switch (type) {
        case R_AARCH64_NONE:
            EL_DEBUG("%s", "R_AARCH64_NONE\n");
            break;
        case R_AARCH64_RELATIVE:
            if (sym) {
                EL_DEBUG("%s", "R_AARCH64_RELATIVE with symbol ref!\n");
                return EL_BADREL;
            }

            EL_DEBUG("Applying R_AARCH64_RELATIVE reloc @%p\n", p);
            *p = rel->r_addend + ctx->base_load_vaddr;
            break;

        default:
            EL_DEBUG("Bad relocation %u\n", type);
            return EL_BADREL;
    }

    return EL_OK;
}

el_status el_applyrel(el_ctx* ctx, Elf_Rel* rel) {
    uintptr_t* p = (uintptr_t*)(rel->r_offset + ctx->base_load_paddr);
    uint32_t type = ELF_R_TYPE(rel->r_info);
    uint32_t sym = ELF_R_SYM(rel->r_info);

    switch (type) {
        case R_AARCH64_NONE:
            EL_DEBUG("%s", "R_AARCH64_NONE\n");
            break;
        case R_AARCH64_RELATIVE:
            if (sym) {
                EL_DEBUG("%s", "R_AARCH64_RELATIVE with symbol ref!\n");
                return EL_BADREL;
            }

            EL_DEBUG("Applying R_AARCH64_RELATIVE reloc @%p\n", p);
            *p += ctx->base_load_vaddr;
            break;

        default:
            EL_DEBUG("Bad relocation %u\n", type);
            return EL_BADREL;
    }

    return EL_OK;
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

static bool fpread(el_ctx* ctx, void* dest, size_t nb, size_t offset) {
    uint8* p = rawElf;
    p += offset;
    memcpy(dest, p, nb);
    return TRUE;
}

static void* alloccb(el_ctx* ctx, Elf_Addr phys, Elf_Addr virt, Elf_Addr size) {
    return (void*)virt;
}

inFuncs_st inFuncs;
outFuncs_st* outFuncs;
void* elfBuf;

int32 mrc_init(void) {
    el_ctx ctx;
    el_status stat;

    showText("gcc mrp v20201111");

    outFuncs = NULL;
    elfBuf = NULL;
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
    // if (posix_memalign(&elfBuf, ctx.align, ctx.memsz)) {
    //     perror("memalign");
    //     return 1;
    // }

    elfBuf = mrc_malloc(ctx.memsz);
    ctx.base_load_vaddr = ctx.base_load_paddr = (uintptr_t)elfBuf;
    stat = el_load(&ctx, alloccb);
    if (stat) {
        EL_DEBUG("loading: error %d\n", stat);
        return MR_FAILED;
    }
    mrc_freeFileData(rawElf, rawElfLen);

    stat = el_relocate(&ctx);
    if (stat) {
        EL_DEBUG("relocating: error %d\n", stat);
        return MR_FAILED;
    }
    {
        _START ep = (_START)(ctx.ehdr.e_entry + (uintptr_t)elfBuf);
        EL_DEBUG("Binary entrypoint is 0x%X; invoking 0x%p\n", ctx.ehdr.e_entry, ep);
        outFuncs = ep(&inFuncs);
        outFuncs->mrc_init();
    }
    return 0;
}

int32 mrc_exitApp(void) {
    int32 ret = MR_SUCCESS;
    if (outFuncs) {
        ret = outFuncs->mrc_exitApp();
    }
    if (elfBuf) {
        mrc_free(elfBuf);
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