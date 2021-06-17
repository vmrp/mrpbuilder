// https://github.com/erincandescent/elfload
/* Copyright © 2014, Owen Shepherd
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include "elfload.h"

#define R_ARM_NONE 0
#define R_ARM_RELATIVE 23

el_status el_applyrela(el_ctx *ctx, Elf_RelA *rel) {
    uintptr_t *p = (uintptr_t *)(rel->r_offset + ctx->base_load_paddr);
    uint32_t type = ELF_R_TYPE(rel->r_info);
    uint32_t sym = ELF_R_SYM(rel->r_info);

    switch (type) {
        case R_ARM_RELATIVE:
            if (sym) {
                EL_DEBUG("%s", "R_ARM_RELATIVE with symbol ref!\n");
                return EL_BADREL;
            }

            EL_DEBUG("Applying R_ARM_RELATIVE reloc @%p\n", p);
            *p = rel->r_addend + ctx->base_load_vaddr;
            break;

        case R_ARM_NONE:
            EL_DEBUG("%s", "R_ARM_NONE\n");
            // break;
        default:
            EL_DEBUG("Bad relocation %u\n", type);
            return EL_BADREL;
    }

    return EL_OK;
}

el_status el_applyrel(el_ctx *ctx, Elf_Rel *rel) {
    uintptr_t *p = (uintptr_t *)(rel->r_offset + ctx->base_load_paddr);
    uint32_t type = ELF_R_TYPE(rel->r_info);
    uint32_t sym = ELF_R_SYM(rel->r_info);

    switch (type) {
        case R_ARM_RELATIVE:
            if (sym) {
                EL_DEBUG("%s", "R_ARM_RELATIVE with symbol ref!\n");
                return EL_BADREL;
            }

            EL_DEBUG("el_applyrel Applying R_ARM_RELATIVE reloc @%p\n", p);
            *p += ctx->base_load_vaddr;
            break;

        case R_ARM_NONE:
            EL_DEBUG("%s", "R_ARM_NONE\n");
            // break;
        default:
            EL_DEBUG("Bad relocation %u\n", type);
            return EL_BADREL;
    }

    return EL_OK;
}

el_status el_pread(el_ctx *ctx, void *def, size_t nb, size_t offset) {
    return ctx->pread(ctx, def, nb, offset) ? EL_OK : EL_EIO;
}

#define EL_PHOFF(ctx, num) (((ctx)->ehdr.e_phoff + (num) * (ctx)->ehdr.e_phentsize))

el_status el_findphdr(el_ctx *ctx, Elf_Phdr *phdr, uint32 type, unsigned *i) {
    el_status rv = EL_OK;
    for (; *i < ctx->ehdr.e_phnum; (*i)++) {
        rv = el_pread(ctx, phdr, sizeof *phdr, EL_PHOFF(ctx, *i));
        if (rv) return rv;

        if (phdr->p_type == type) {
            return rv;
        }
    }

    *i = -1;
    return rv;
}

el_status el_init(el_ctx *ctx) {
    el_status rv = EL_OK;

    rv = el_pread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0);
    if (rv) return rv;

    /* validate header */

    if (!IS_ELF(ctx->ehdr))
        return EL_NOTELF;

    if (ctx->ehdr.e_ident[EI_CLASS] != ELFCLASS)
        return EL_WRONGBITS;

    if (ctx->ehdr.e_ident[EI_DATA] != ELFDATATHIS)
        return EL_WRONGENDIAN;

    if (ctx->ehdr.e_ident[EI_VERSION] != EV_CURRENT)
        return EL_NOTELF;

    if (ctx->ehdr.e_type != ET_EXEC && ctx->ehdr.e_type != ET_DYN)
        return EL_NOTEXEC;

    if (ctx->ehdr.e_machine != EM_THIS)
        return EL_WRONGARCH;

    if (ctx->ehdr.e_version != EV_CURRENT)
        return EL_NOTELF;

    /* iterate through, calculate extents */
    ctx->base_load_paddr = ctx->base_load_vaddr = 0;
    ctx->align = 1;
    ctx->memsz = 0;

    if (ctx->ehdr.e_phentsize != sizeof(Elf_Phdr))
        return EL_NOTELF;

    { /* load phdrs */
        unsigned i = 0;
        for (;;) {
            Elf_Phdr ph;
            Elf_Addr phend;

            rv = el_findphdr(ctx, &ph, PT_LOAD, &i);
            if (rv) return rv;

            if (i == (unsigned)-1)
                break;

            phend = ph.p_vaddr + ph.p_memsz;
            if (phend > ctx->memsz)
                ctx->memsz = phend;

            if (ph.p_align > ctx->align)
                ctx->align = ph.p_align;

            i++;
        }
    }
    if (ctx->ehdr.e_type == ET_DYN) {
        Elf_Phdr ph;
        unsigned i = 0;
        rv = el_findphdr(ctx, &ph, PT_DYNAMIC, &i);
        if (rv) return rv;

        if (i == (unsigned)-1)
            return EL_NODYN;

        ctx->dynoff = ph.p_offset;
        ctx->dynsize = ph.p_filesz;
    } else {
        ctx->dynoff = 0;
        ctx->dynsize = 0;
    }
    return rv;
}

el_status el_load(el_ctx *ctx, el_alloc_cb alloc) {
    Elf_Addr pload, vload;
    char *dest;
    el_status rv = EL_OK;

    /* address deltas */
    Elf_Addr pdelta = ctx->base_load_paddr;
    Elf_Addr vdelta = ctx->base_load_vaddr;

    /* iterate paddrs */
    Elf_Phdr ph;
    unsigned i = 0;
    for (;;) {
        rv = el_findphdr(ctx, &ph, PT_LOAD, &i);
        if (rv) return rv;

        if (i == (unsigned)-1)
            break;

        pload = ph.p_paddr + pdelta;
        vload = ph.p_vaddr + vdelta;

        /* allocate mem */
        dest = alloc(ctx, pload, vload, ph.p_memsz);
        if (!dest) return EL_ENOMEM;

        EL_DEBUG("Loading seg fileoff %x, vaddr %x to %p\n", ph.p_offset, ph.p_vaddr, dest);

        /* read loaded portion */
        rv = el_pread(ctx, dest, ph.p_filesz, ph.p_offset);
        if (rv) return rv;

        /* zero mem-only portion */
        mrc_memset(dest + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);

        i++;
    }

    return rv;
}

el_status el_finddyn(el_ctx *ctx, Elf_Dyn *dyn, uint32 tag) {
    el_status rv = EL_OK;
    size_t i;
    size_t ndyn = ctx->dynsize / sizeof(Elf_Dyn);

    for (i = 0; i < ndyn; i++) {
        rv = el_pread(ctx, dyn, sizeof *dyn, ctx->dynoff + i * sizeof *dyn);
        if (rv) return rv;

        if (dyn->d_tag == tag)
            return EL_OK;
    }

    dyn->d_tag = DT_NULL;
    return EL_OK;
}

el_status el_findrelocs(el_ctx *ctx, el_relocinfo *ri, uint32 type) {
    el_status rv = EL_OK;

    Elf_Dyn rel, relsz, relent;
    rv = el_finddyn(ctx, &rel, type);
    if (rv) return rv;

    rv = el_finddyn(ctx, &relsz, type + 1);
    if (rv) return rv;

    rv = el_finddyn(ctx, &relent, type + 2);
    if (rv) return rv;

    if (rel.d_tag == DT_NULL || relsz.d_tag == DT_NULL || relent.d_tag == DT_NULL) {
        ri->entrysize = 0;
        ri->tablesize = 0;
        ri->tableoff = 0;
    } else {
        ri->tableoff = rel.d_un.d_ptr;
        ri->tablesize = relsz.d_un.d_val;
        ri->entrysize = relent.d_un.d_val;
    }

    return rv;
}

extern el_status el_applyrel(el_ctx *ctx, Elf_Rel *rel);
extern el_status el_applyrela(el_ctx *ctx, Elf_RelA *rela);

el_status el_relocate(el_ctx *ctx) {
    char *base;
    el_status rv = EL_OK;
    el_relocinfo ri;
    size_t cnt, i;
    Elf_Rel *reltab;
    Elf_RelA *relatab;

    // not dynamic
    if (ctx->ehdr.e_type != ET_DYN)
        return EL_OK;

    base = (char *)ctx->base_load_paddr;

#ifdef EL_ARCH_USES_REL
    rv = el_findrelocs(ctx, &ri, DT_REL);
    if (rv) return rv;

    if (ri.entrysize != sizeof(Elf_Rel) && ri.tablesize) {
        EL_DEBUG("Relocation size %u doesn't match expected %u\n", ri.entrysize, sizeof(Elf_Rel));
        return EL_BADREL;
    }

    cnt = ri.tablesize / sizeof(Elf_Rel);
    reltab = (Elf_Rel *)(base + ri.tableoff);
    for (i = 0; i < cnt; i++) {
        rv = el_applyrel(ctx, &reltab[i]);
        if (rv) return rv;
    }
#else
    EL_DEBUG("Architecture doesn't use REL\n");
#endif

#ifdef EL_ARCH_USES_RELA
    rv = el_findrelocs(ctx, &ri, DT_RELA);
    if (rv) return rv;

    if (ri.entrysize != sizeof(Elf_RelA) && ri.tablesize) {
        EL_DEBUG("Relocation size %u doesn't match expected %u\n", ri.entrysize, sizeof(Elf_RelA));
        return EL_BADREL;
    }

    cnt = ri.tablesize / sizeof(Elf_RelA);
    relatab = (Elf_RelA *)(base + ri.tableoff);
    for (i = 0; i < cnt; i++) {
        rv = el_applyrela(ctx, &relatab[i]);
        if (rv) return rv;
    }
#else
    EL_DEBUG("%s", "Architecture doesn't use RELA\n");
#endif

#if !defined(EL_ARCH_USES_REL) && !defined(EL_ARCH_USES_RELA)
#error No relocation type defined!
#endif

    return rv;
}
