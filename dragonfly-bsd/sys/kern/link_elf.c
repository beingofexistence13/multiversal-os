/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/link_elf.c,v 1.24 1999/12/24 15:33:36 bde Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/nlookup.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/linker.h>
#include <machine/elf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_zone.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#if defined(__x86_64__) && defined(_KERNEL_VIRTUAL)
#include <stdio.h>
#endif

static int	link_elf_preload_file(const char *, linker_file_t *);
static int	link_elf_preload_finish(linker_file_t);
static int	link_elf_load_file(const char*, linker_file_t*);
static int	link_elf_lookup_symbol(linker_file_t, const char*,
				       c_linker_sym_t*);
static int	link_elf_symbol_values(linker_file_t, c_linker_sym_t, linker_symval_t*);
static int	link_elf_search_symbol(linker_file_t, caddr_t value,
				       c_linker_sym_t* sym, long* diffp);

static void	link_elf_unload_file(linker_file_t);
static void	link_elf_unload_module(linker_file_t);
static int	link_elf_lookup_set(linker_file_t, const char *,
			void ***, void ***, int *);
static int	elf_lookup(linker_file_t lf, Elf_Size symidx, int deps, Elf_Addr *);
static void link_elf_reloc_local(linker_file_t lf);

static struct linker_class_ops link_elf_class_ops = {
    link_elf_load_file,
    link_elf_preload_file,
};

static struct linker_file_ops link_elf_file_ops = {
    .lookup_symbol = link_elf_lookup_symbol,
    .symbol_values = link_elf_symbol_values,
    .search_symbol = link_elf_search_symbol,
    .unload = link_elf_unload_file,
    .lookup_set = link_elf_lookup_set
};

static struct linker_file_ops link_elf_module_ops = {
    .lookup_symbol = link_elf_lookup_symbol,
    .symbol_values = link_elf_symbol_values,
    .search_symbol = link_elf_search_symbol,
    .preload_finish = link_elf_preload_finish,
    .unload = link_elf_unload_module,
    .lookup_set = link_elf_lookup_set,
};

typedef struct elf_file {
    caddr_t		address;	/* Relocation address */
    const Elf_Dyn*	dynamic;	/* Symbol table etc. */
    Elf_Hashelt		nbuckets;	/* DT_HASH info */
    Elf_Hashelt		nchains;
    const Elf_Hashelt*	buckets;
    const Elf_Hashelt*	chains;
    caddr_t		hash;
    caddr_t		strtab;		/* DT_STRTAB */
    int			strsz;		/* DT_STRSZ */
    const Elf_Sym*	symtab;		/* DT_SYMTAB */
    Elf_Addr*		got;		/* DT_PLTGOT */
    const Elf_Rel*	pltrel;		/* DT_JMPREL */
    int			pltrelsize;	/* DT_PLTRELSZ */
    const Elf_Rela*	pltrela;	/* DT_JMPREL */
    int			pltrelasize;	/* DT_PLTRELSZ */
    const Elf_Rel*	rel;		/* DT_REL */
    int			relsize;	/* DT_RELSZ */
    const Elf_Rela*	rela;		/* DT_RELA */
    int			relasize;	/* DT_RELASZ */
    caddr_t		modptr;
    const Elf_Sym*	ddbsymtab;	/* The symbol table we are using */
    long		ddbsymcnt;	/* Number of symbols */
    caddr_t		ddbstrtab;	/* String table */
    long		ddbstrcnt;	/* number of bytes in string table */
    caddr_t		symbase;	/* malloc'ed symbold base */
    caddr_t		strbase;	/* malloc'ed string base */
} *elf_file_t;

static int		parse_dynamic(linker_file_t lf);
static int		relocate_file(linker_file_t lf);
static int		parse_module_symbols(linker_file_t lf);

/*
 * The kernel symbol table starts here.
 */
extern struct _dynamic _DYNAMIC;

static void
link_elf_init(void* arg)
{
    Elf_Dyn	*dp;
    caddr_t	modptr, baseptr, sizeptr;
    elf_file_t	ef;
    char	*modname;

#if ELF_TARG_CLASS == ELFCLASS32
    linker_add_class("elf32", NULL, &link_elf_class_ops);
#else
    linker_add_class("elf64", NULL, &link_elf_class_ops);
#endif

    dp = (Elf_Dyn*) &_DYNAMIC;
    if (dp) {
	ef = kmalloc(sizeof(struct elf_file), M_LINKER, M_INTWAIT | M_ZERO);
	ef->address = NULL;
	ef->dynamic = dp;
	modname = NULL;
	modptr = preload_search_by_type("elf kernel");
	if (modptr)
	    modname = (char *)preload_search_info(modptr, MODINFO_NAME);
	if (modname == NULL)
	    modname = "kernel";
	linker_kernel_file = linker_make_file(modname, ef, &link_elf_file_ops);
	if (linker_kernel_file == NULL)
	    panic("link_elf_init: Can't create linker structures for kernel");
	parse_dynamic(linker_kernel_file);
#if defined(__x86_64__) && defined(_KERNEL_VIRTUAL)
	fprintf(stderr, "WARNING: KERNBASE being used\n");
#endif
	linker_kernel_file->address = (caddr_t) KERNBASE;
	linker_kernel_file->size = -(intptr_t)linker_kernel_file->address;

	if (modptr) {
	    ef->modptr = modptr;
	    baseptr = preload_search_info(modptr, MODINFO_ADDR);
	    if (baseptr)
		linker_kernel_file->address = *(caddr_t *)baseptr;
	    sizeptr = preload_search_info(modptr, MODINFO_SIZE);
	    if (sizeptr)
		linker_kernel_file->size = *(size_t *)sizeptr;
	}
	parse_module_symbols(linker_kernel_file);
	linker_current_file = linker_kernel_file;
	linker_kernel_file->flags |= LINKER_FILE_LINKED;
    }
}

SYSINIT(link_elf, SI_BOOT2_KLD, SI_ORDER_SECOND, link_elf_init, 0);

static int
parse_module_symbols(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    caddr_t	pointer;
    caddr_t	ssym, esym, base;
    caddr_t	strtab;
    int		strcnt;
    Elf_Sym*	symtab;
    int		symcnt;

    if (ef->modptr == NULL)
	return 0;
    pointer = preload_search_info(ef->modptr, MODINFO_METADATA|MODINFOMD_SSYM);
    if (pointer == NULL)
	return 0;
    ssym = *(caddr_t *)pointer;
    pointer = preload_search_info(ef->modptr, MODINFO_METADATA|MODINFOMD_ESYM);
    if (pointer == NULL)
	return 0;
    esym = *(caddr_t *)pointer;

    base = ssym;

    symcnt = *(long *)base;
    base += sizeof(long);
    symtab = (Elf_Sym *)base;
    base += roundup(symcnt, sizeof(long));

    if (base > esym || base < ssym) {
	kprintf("Symbols are corrupt!\n");
	return EINVAL;
    }

    strcnt = *(long *)base;
    base += sizeof(long);
    strtab = base;
    base += roundup(strcnt, sizeof(long));

    if (base > esym || base < ssym) {
	kprintf("Symbols are corrupt!\n");
	return EINVAL;
    }

    ef->ddbsymtab = symtab;
    ef->ddbsymcnt = symcnt / sizeof(Elf_Sym);
    ef->ddbstrtab = strtab;
    ef->ddbstrcnt = strcnt;

    return 0;
}

static int
parse_dynamic(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    const Elf_Dyn *dp;
    int plttype = DT_REL;

    for (dp = ef->dynamic; dp->d_tag != DT_NULL; dp++) {
	switch (dp->d_tag) {
	case DT_HASH:
	{
	    /* From src/libexec/rtld-elf/rtld.c */
	    const Elf_Hashelt *hashtab = (const Elf_Hashelt *)
		(ef->address + dp->d_un.d_ptr);
	    ef->nbuckets = hashtab[0];
	    ef->nchains = hashtab[1];
	    ef->buckets = hashtab + 2;
	    ef->chains = ef->buckets + ef->nbuckets;
	    break;
	}
	case DT_STRTAB:
	    ef->strtab = (caddr_t) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_STRSZ:
	    ef->strsz = dp->d_un.d_val;
	    break;
	case DT_SYMTAB:
	    ef->symtab = (Elf_Sym*) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_SYMENT:
	    if (dp->d_un.d_val != sizeof(Elf_Sym))
		return ENOEXEC;
	    break;
	case DT_PLTGOT:
	    ef->got = (Elf_Addr *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_REL:
	    ef->rel = (const Elf_Rel *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_RELSZ:
	    ef->relsize = dp->d_un.d_val;
	    break;
	case DT_RELENT:
	    if (dp->d_un.d_val != sizeof(Elf_Rel))
		return ENOEXEC;
	    break;
	case DT_JMPREL:
	    ef->pltrel = (const Elf_Rel *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_PLTRELSZ:
	    ef->pltrelsize = dp->d_un.d_val;
	    break;
	case DT_RELA:
	    ef->rela = (const Elf_Rela *) (ef->address + dp->d_un.d_ptr);
	    break;
	case DT_RELASZ:
	    ef->relasize = dp->d_un.d_val;
	    break;
	case DT_RELAENT:
	    if (dp->d_un.d_val != sizeof(Elf_Rela))
		return ENOEXEC;
	    break;
	case DT_PLTREL:
	    plttype = dp->d_un.d_val;
	    if (plttype != DT_REL && plttype != DT_RELA)
		return ENOEXEC;
	    break;
	}
    }

    if (plttype == DT_RELA) {
	ef->pltrela = (const Elf_Rela *) ef->pltrel;
	ef->pltrel = NULL;
	ef->pltrelasize = ef->pltrelsize;
	ef->pltrelsize = 0;
    }

    ef->ddbsymtab = ef->symtab;
    ef->ddbsymcnt = ef->nchains;
    ef->ddbstrtab = ef->strtab;
    ef->ddbstrcnt = ef->strsz;

    return 0;
}

static void
link_elf_error(const char *s)
{
    kprintf("kldload: %s\n", s);
}

static int
link_elf_preload_file(const char *filename, linker_file_t *result)
{
    caddr_t		modptr, baseptr, sizeptr, dynptr;
    char		*type;
    elf_file_t		ef;
    linker_file_t	lf;
    int			error;
    vm_offset_t		dp;

    /*
     * Look to see if we have the module preloaded.
     */
    modptr = preload_search_by_name(filename);
    if (modptr == NULL)
	return ENOENT;

    /* It's preloaded, check we can handle it and collect information */
    type = (char *)preload_search_info(modptr, MODINFO_TYPE);
    baseptr = preload_search_info(modptr, MODINFO_ADDR);
    sizeptr = preload_search_info(modptr, MODINFO_SIZE);
    dynptr = preload_search_info(modptr, MODINFO_METADATA|MODINFOMD_DYNAMIC);
    if (type == NULL ||
	    (strcmp(type, "elf" __XSTRING(__ELF_WORD_SIZE) " module") != 0 &&
	    strcmp(type, "elf module") != 0))
	return (EFTYPE);
    if (baseptr == NULL || sizeptr == NULL || dynptr == NULL)
	return (EINVAL);

    ef = kmalloc(sizeof(struct elf_file), M_LINKER, M_WAITOK | M_ZERO);
    ef->modptr = modptr;
    ef->address = *(caddr_t *)baseptr;
    dp = (vm_offset_t)ef->address + *(vm_offset_t *)dynptr;
    ef->dynamic = (Elf_Dyn *)dp;
    lf = linker_make_file(filename, ef, &link_elf_module_ops);
    if (lf == NULL) {
	kfree(ef, M_LINKER);
	return ENOMEM;
    }
    lf->address = ef->address;
    lf->size = *(size_t *)sizeptr;

    error = parse_dynamic(lf);
    if (error) {
	linker_file_unload(lf);
	return error;
    }
    link_elf_reloc_local(lf);
    *result = lf;
    return (0);
}

static int
link_elf_preload_finish(linker_file_t lf)
{
    int			error;

    error = relocate_file(lf);
    if (error)
	return error;
    parse_module_symbols(lf);

    return (0);
}

static int
link_elf_load_file(const char* filename, linker_file_t* result)
{
    struct nlookupdata nd;
    struct thread *td = curthread;	/* XXX */
    struct proc *p = td->td_proc;
    struct vnode *vp;
    Elf_Ehdr *hdr;
    caddr_t firstpage;
    int nbytes, i;
    Elf_Phdr *phdr;
    Elf_Phdr *phlimit;
    Elf_Phdr *segs[2];
    int nsegs;
    Elf_Phdr *phdyn;
    caddr_t mapbase;
    size_t mapsize;
    Elf_Addr base_vaddr;
    Elf_Addr base_vlimit;
    int error = 0;
    int resid;
    elf_file_t ef;
    linker_file_t lf;
    char *pathname;
    Elf_Shdr *shdr;
    int symtabindex;
    int symstrindex;
    int symcnt;
    int strcnt;

    /* XXX Hack for firmware loading where p == NULL */
    if (p == NULL) {
	p = &proc0;
    }

    KKASSERT(p != NULL);
    if (p->p_ucred == NULL) {
	kprintf("link_elf_load_file: cannot load '%s' from filesystem"
		" this early\n", filename);
	return ENOENT;
    }
    shdr = NULL;
    lf = NULL;
    pathname = linker_search_path(filename);
    if (pathname == NULL)
	return ENOENT;

    error = nlookup_init(&nd, pathname, UIO_SYSSPACE, NLC_FOLLOW|NLC_LOCKVP);
    if (error == 0)
	error = vn_open(&nd, NULL, FREAD, 0);
    kfree(pathname, M_LINKER);
    if (error) {
	nlookup_done(&nd);
	return error;
    }
    vp = nd.nl_open_vp;
    nd.nl_open_vp = NULL;
    nlookup_done(&nd);

    /*
     * Read the elf header from the file.
     */
    firstpage = kmalloc(PAGE_SIZE, M_LINKER, M_WAITOK);
    hdr = (Elf_Ehdr *)firstpage;
    error = vn_rdwr(UIO_READ, vp, firstpage, PAGE_SIZE, 0,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid);
    nbytes = PAGE_SIZE - resid;
    if (error)
	goto out;

    if (!IS_ELF(*hdr)) {
	error = ENOEXEC;
	goto out;
    }

    if (hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS
      || hdr->e_ident[EI_DATA] != ELF_TARG_DATA) {
	link_elf_error("Unsupported file layout");
	error = ENOEXEC;
	goto out;
    }
    if (hdr->e_ident[EI_VERSION] != EV_CURRENT
      || hdr->e_version != EV_CURRENT) {
	link_elf_error("Unsupported file version");
	error = ENOEXEC;
	goto out;
    }
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
	error = ENOSYS;
	goto out;
    }
    if (hdr->e_machine != ELF_TARG_MACH) {
	link_elf_error("Unsupported machine");
	error = ENOEXEC;
	goto out;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (!((hdr->e_phentsize == sizeof(Elf_Phdr)) &&
	  (hdr->e_phoff + hdr->e_phnum*sizeof(Elf_Phdr) <= PAGE_SIZE) &&
	  (hdr->e_phoff + hdr->e_phnum*sizeof(Elf_Phdr) <= nbytes)))
	link_elf_error("Unreadable program headers");

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) (firstpage + hdr->e_phoff);
    phlimit = phdr + hdr->e_phnum;
    nsegs = 0;
    phdyn = NULL;
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_LOAD:
	    if (nsegs == 2) {
		link_elf_error("Too many sections");
		error = ENOEXEC;
		goto out;
	    }
	    segs[nsegs] = phdr;
	    ++nsegs;
	    break;

	case PT_PHDR:
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;

	case PT_INTERP:
	    error = ENOSYS;
	    goto out;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	link_elf_error("Object is not dynamically-linked");
	error = ENOEXEC;
	goto out;
    }

    /*
     * Allocate the entire address space of the object, to stake out our
     * contiguous region, and to establish the base address for relocation.
     */
    base_vaddr = trunc_page(segs[0]->p_vaddr);
    base_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_memsz);
    mapsize = base_vlimit - base_vaddr;

    ef = kmalloc(sizeof(struct elf_file), M_LINKER, M_WAITOK | M_ZERO);
    ef->address = kmalloc(mapsize, M_LINKER, M_WAITOK);
    mapbase = ef->address;

    /*
     * Read the text and data sections and zero the bss.
     */
    for (i = 0; i < 2; i++) {
	caddr_t segbase = mapbase + segs[i]->p_vaddr - base_vaddr;
	error = vn_rdwr(UIO_READ, vp,
			segbase, segs[i]->p_filesz, segs[i]->p_offset,
			UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid);
	if (error) {
	    kfree(ef->address, M_LINKER);
	    kfree(ef, M_LINKER);
	    goto out;
	}
	bzero(segbase + segs[i]->p_filesz,
	      segs[i]->p_memsz - segs[i]->p_filesz);
    }

    ef->dynamic = (const Elf_Dyn *) (mapbase + phdyn->p_vaddr - base_vaddr);

    lf = linker_make_file(filename, ef, &link_elf_file_ops);
    if (lf == NULL) {
	kfree(ef->address, M_LINKER);
	kfree(ef, M_LINKER);
	error = ENOMEM;
	goto out;
    }
    lf->address = ef->address;
    lf->size = mapsize;

    error = parse_dynamic(lf);
    if (error)
	goto out;
    link_elf_reloc_local(lf);
    error = linker_load_dependencies(lf);
    if (error)
	goto out;
    error = relocate_file(lf);
    if (error)
	goto out;

    /* Try and load the symbol table if it's present.  (you can strip it!) */
    nbytes = hdr->e_shnum * hdr->e_shentsize;
    if (nbytes == 0 || hdr->e_shoff == 0)
	goto nosyms;
    shdr = kmalloc(nbytes, M_LINKER, M_WAITOK | M_ZERO);
    error = vn_rdwr(UIO_READ, vp,
		    (caddr_t)shdr, nbytes, hdr->e_shoff,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid);
    if (error)
	goto out;
    symtabindex = -1;
    symstrindex = -1;
    for (i = 0; i < hdr->e_shnum; i++) {
	if (shdr[i].sh_type == SHT_SYMTAB) {
	    symtabindex = i;
	    symstrindex = shdr[i].sh_link;
	}
    }
    if (symtabindex < 0 || symstrindex < 0)
	goto nosyms;

    symcnt = shdr[symtabindex].sh_size;
    ef->symbase = kmalloc(symcnt, M_LINKER, M_WAITOK);
    strcnt = shdr[symstrindex].sh_size;
    ef->strbase = kmalloc(strcnt, M_LINKER, M_WAITOK);
    error = vn_rdwr(UIO_READ, vp,
		    ef->symbase, symcnt, shdr[symtabindex].sh_offset,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid);
    if (error)
	goto out;
    error = vn_rdwr(UIO_READ, vp,
		    ef->strbase, strcnt, shdr[symstrindex].sh_offset,
		    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid);
    if (error)
	goto out;

    ef->ddbsymcnt = symcnt / sizeof(Elf_Sym);
    ef->ddbsymtab = (const Elf_Sym *)ef->symbase;
    ef->ddbstrcnt = strcnt;
    ef->ddbstrtab = ef->strbase;

nosyms:

    *result = lf;

out:
    if (error && lf)
	linker_file_unload(lf);
    if (shdr)
	kfree(shdr, M_LINKER);
    if (firstpage)
	kfree(firstpage, M_LINKER);
    vn_unlock(vp);
    vn_close(vp, FREAD, NULL);

    return error;
}

Elf_Addr
elf_relocaddr(linker_file_t lf, Elf_Addr x)
{
#if 0
    elf_file_t ef;

    ef = lf->priv;
    if (x >= ef->pcpu_start && x < ef->pcpu_stop)
	return ((x - ef->pcpu_start) + ef->pcpu_base);
#ifdef VIMAGE
    if (x >= ef->vnet_start && x < ef->vnet_stop)
	return ((x - ef->vnet_start) + ef->vnet_base);
#endif
#endif
    return (x);
}

static void
link_elf_unload_file(linker_file_t file)
{
    elf_file_t ef = file->priv;

    if (ef) {
	if (ef->address)
	    kfree(ef->address, M_LINKER);
	if (ef->symbase)
	    kfree(ef->symbase, M_LINKER);
	if (ef->strbase)
	    kfree(ef->strbase, M_LINKER);
	kfree(ef, M_LINKER);
    }
}

static void
link_elf_unload_module(linker_file_t file)
{
    elf_file_t ef = file->priv;

    if (ef)
	kfree(ef, M_LINKER);
    if (file->pathname)
	preload_delete_name(file->pathname);
}

static const char *
symbol_name(elf_file_t ef, Elf_Size r_info)
{
    const Elf_Sym *ref;

    if (ELF_R_SYM(r_info)) {
	ref = ef->symtab + ELF_R_SYM(r_info);
	return ef->strtab + ref->st_name;
    } else
	return NULL;
}

static int
relocate_file(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    const Elf_Rel *rellim;
    const Elf_Rel *rel;
    const Elf_Rela *relalim;
    const Elf_Rela *rela;
    const char *symname;

    /* Perform relocations without addend if there are any: */
    rel = ef->rel;
    if (rel) {
	rellim = (const Elf_Rel *)((const char *)ef->rel + ef->relsize);
	while (rel < rellim) {
	    if (elf_reloc(lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL, elf_lookup)) {
		symname = symbol_name(ef, rel->r_info);
		kprintf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    rela = ef->rela;
    if (rela) {
	relalim = (const Elf_Rela *)((const char *)ef->rela + ef->relasize);
	while (rela < relalim) {
	    if (elf_reloc(lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA, elf_lookup)) {
		symname = symbol_name(ef, rela->r_info);
		kprintf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rela++;
	}
    }

    /* Perform PLT relocations without addend if there are any: */
    rel = ef->pltrel;
    if (rel) {
	rellim = (const Elf_Rel *)((const char *)ef->pltrel + ef->pltrelsize);
	while (rel < rellim) {
	    if (elf_reloc(lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL, elf_lookup)) {
		symname = symbol_name(ef, rel->r_info);
		kprintf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    rela = ef->pltrela;
    if (rela) {
	relalim = (const Elf_Rela *)((const char *)ef->pltrela + ef->pltrelasize);
	while (rela < relalim) {
	    symname = symbol_name(ef, rela->r_info);
	    if (elf_reloc(lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA, elf_lookup)) {
		kprintf("link_elf: symbol %s undefined\n", symname);
		return ENOENT;
	    }
	    rela++;
	}
    }

    return 0;
}

/*
 * Hash function for symbol table lookup.  Don't even think about changing
 * this.  It is specified by the System V ABI.
 */
static unsigned long
elf_hash(const char *name)
{
    const unsigned char *p = (const unsigned char *) name;
    unsigned long h = 0;
    unsigned long g;

    while (*p != '\0') {
	h = (h << 4) + *p++;
	if ((g = h & 0xf0000000) != 0)
	    h ^= g >> 24;
	h &= ~g;
    }
    return h;
}

static int
link_elf_lookup_symbol(linker_file_t lf, const char* name, c_linker_sym_t* sym)
{
    elf_file_t ef = lf->priv;
    unsigned long symnum;
    const Elf_Sym* symp;
    const char *strp;
    unsigned long hash;
    int i;

    /* If we don't have a hash, bail. */
    if (ef->buckets == NULL || ef->nbuckets == 0) {
	kprintf("link_elf_lookup_symbol: missing symbol hash table\n");
	return ENOENT;
    }

    /* First, search hashed global symbols */
    hash = elf_hash(name);
    symnum = ef->buckets[hash % ef->nbuckets];

    while (symnum != STN_UNDEF) {
	if (symnum >= ef->nchains) {
	    kprintf("link_elf_lookup_symbol: corrupt symbol table\n");
	    return ENOENT;
	}

	symp = ef->symtab + symnum;
	if (symp->st_name == 0) {
	    kprintf("link_elf_lookup_symbol: corrupt symbol table\n");
	    return ENOENT;
	}

	strp = ef->strtab + symp->st_name;

	if (strcmp(name, strp) == 0) {
	    if (symp->st_shndx != SHN_UNDEF ||
		(symp->st_value != 0 &&
		 ELF_ST_TYPE(symp->st_info) == STT_FUNC)
	     ) {
		*sym = (c_linker_sym_t) symp;
		return 0;
	    } else {
		return ENOENT;
	    }
	}

	symnum = ef->chains[symnum];
    }

    /* If we have not found it, look at the full table (if loaded) */
    if (ef->symtab == ef->ddbsymtab)
	return ENOENT;

    /* Exhaustive search */
    for (i = 0, symp = ef->ddbsymtab; i < ef->ddbsymcnt; i++, symp++) {
	strp = ef->ddbstrtab + symp->st_name;
	if (strcmp(name, strp) == 0) {
	    if (symp->st_shndx != SHN_UNDEF ||
		(symp->st_value != 0 &&
		 ELF_ST_TYPE(symp->st_info) == STT_FUNC)) {
		*sym = (c_linker_sym_t) symp;
		return 0;
	    } else {
		return ENOENT;
	    }
	}
    }
    return ENOENT;
}

static int
link_elf_symbol_values(linker_file_t lf, c_linker_sym_t sym, linker_symval_t *symval)
{
    elf_file_t	    ef = lf->priv;
    const Elf_Sym  *es = (const Elf_Sym *)sym;

    symval->value = 0;	/* avoid gcc warnings */

    if (es >= ef->symtab && es < (ef->symtab + ef->nchains)) {
	symval->name = ef->strtab + es->st_name;
	symval->value = ef->address + es->st_value;
	symval->size = es->st_size;
	return 0;
    }
    if (ef->symtab == ef->ddbsymtab)
	return ENOENT;
    if (es >= ef->ddbsymtab && es < (ef->ddbsymtab + ef->ddbsymcnt)) {
	symval->name = ef->ddbstrtab + es->st_name;
	symval->value = ef->address + es->st_value;
	symval->size = es->st_size;
	return 0;
    }
    return ENOENT;
}

static int
link_elf_search_symbol(linker_file_t lf, caddr_t value,
		       c_linker_sym_t *sym, long *diffp)
{
    elf_file_t	    ef = lf->priv;
    u_long	    off = (uintptr_t)(void *)value;
    u_long	    diff = off;
    u_long	    st_value;
    const Elf_Sym  *es;
    const Elf_Sym  *best = NULL;
    int		    i;

    for (i = 0, es = ef->ddbsymtab; i < ef->ddbsymcnt; i++, es++) {
	if (es->st_name == 0)
	    continue;
	st_value = es->st_value + (uintptr_t)(void *)ef->address;
	if (off >= st_value) {
	    if (off - st_value < diff) {
		diff = off - st_value;
		best = es;
		if (diff == 0)
		    break;
	    } else if (off - st_value == diff) {
		best = es;
	    }
	}
    }
    if (best == NULL)
	*diffp = off;
    else
	*diffp = diff;
    *sym = (c_linker_sym_t) best;

    return 0;
}

/*
 * Look up a linker set on an ELF system.
 */
static int
link_elf_lookup_set(linker_file_t lf, const char *name,
		    void ***startp, void ***stopp, int *countp)
{
    c_linker_sym_t  sym;
    linker_symval_t symval;
    char           *setsym;
    void          **start, **stop;
    int		    len, error = 0, count;

    len = strlen(name) + sizeof("__start_set_");	/* sizeof includes \0 */
    setsym = kmalloc(len, M_LINKER, M_WAITOK);

    /* get address of first entry */
    ksnprintf(setsym, len, "%s%s", "__start_set_", name);
    error = link_elf_lookup_symbol(lf, setsym, &sym);
    if (error)
	goto out;
    link_elf_symbol_values(lf, sym, &symval);
    if (symval.value == NULL) {
	error = ESRCH;
	goto out;
    }
    start = (void **)symval.value;

    /* get address of last entry */
    ksnprintf(setsym, len, "%s%s", "__stop_set_", name);
    error = link_elf_lookup_symbol(lf, setsym, &sym);
    if (error)
	goto out;
    link_elf_symbol_values(lf, sym, &symval);
    if (symval.value == NULL) {
	error = ESRCH;
	goto out;
    }
    stop = (void **)symval.value;

    /* and the number of entries */
    count = stop - start;

    /* and copy out */
    if (startp)
	*startp = start;
    if (stopp)
	*stopp = stop;
    if (countp)
	*countp = count;

out:
    kfree(setsym, M_LINKER);
    return error;
}

/*
 * Symbol lookup function that can be used when the symbol index is known (ie
 * in relocations). It uses the symbol index instead of doing a fully fledged
 * hash table based lookup when such is valid. For example for local symbols.
 * This is not only more efficient, it's also more correct. It's not always
 * the case that the symbol can be found through the hash table.
 */
static int
elf_lookup(linker_file_t lf, Elf_Size symidx, int deps, Elf_Addr *result)
{
    elf_file_t	    ef = lf->priv;
    const Elf_Sym  *sym;
    const char     *symbol;

    /* Don't even try to lookup the symbol if the index is bogus. */
    if (symidx >= ef->nchains)
	return (ENOENT);

    sym = ef->symtab + symidx;

    /*
     * Don't do a full lookup when the symbol is local. It may even
     * fail because it may not be found through the hash table.
     */
    if (ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
	/* Force lookup failure when we have an insanity. */
	if (sym->st_shndx == SHN_UNDEF || sym->st_value == 0)
	    return (ENOENT);
	return ((Elf_Addr) ef->address + sym->st_value);
    }
    /*
     * XXX we can avoid doing a hash table based lookup for global
     * symbols as well. This however is not always valid, so we'll
     * just do it the hard way for now. Performance tweaks can
     * always be added.
     */

    symbol = ef->strtab + sym->st_name;

    /* Force a lookup failure if the symbol name is bogus. */
    if (*symbol == 0)
	return (ENOENT);

    return (linker_file_lookup_symbol(lf, symbol, deps, (caddr_t *)result));
}
static void
link_elf_reloc_local(linker_file_t lf)
{
    elf_file_t ef = lf->priv;
    const Elf_Rel *rellim;
    const Elf_Rel *rel;
    const Elf_Rela *relalim;
    const Elf_Rela *rela;

    /* Perform relocations without addend if there are any: */
    if ((rel = ef->rel) != NULL) {
	rellim = (const Elf_Rel *)((const char *)ef->rel + ef->relsize);
	while (rel < rellim) {
	    elf_reloc_local(lf, (Elf_Addr)ef->address, rel, ELF_RELOC_REL,
			    elf_lookup);
	    rel++;
	}
    }

    /* Perform relocations with addend if there are any: */
    if ((rela = ef->rela) != NULL) {
	relalim = (const Elf_Rela *)((const char *)ef->rela + ef->relasize);
	while (rela < relalim) {
	    elf_reloc_local(lf, (Elf_Addr)ef->address, rela, ELF_RELOC_RELA,
			    elf_lookup);
	    rela++;
	}
    }
}
