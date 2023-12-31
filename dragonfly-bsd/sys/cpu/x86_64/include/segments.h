/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2008 The DragonFly Project.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)segments.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD: src/sys/i386/include/segments.h,v 1.24 1999/12/29 04:33:07 peter Exp $
 */

#ifndef _CPU_SEGMENTS_H_
#define _CPU_SEGMENTS_H_

#ifndef LOCORE
#include <sys/tls.h>
#endif

/*
 * x86_64 Segmentation Data Structures and definitions
 */

/*
 * Selectors
 */

#define	SEL_RPL_MASK	3		/* requester's privilege level mask */
#define	ISPL(s)		((s) & SEL_RPL_MASK)	/* privilege level of a selector */
#define	SEL_KPL		0		/* kernel privilege level */
#define	SEL_UPL		3		/* user privilege level */
#define	SEL_LDT		4		/* local descriptor table */
#define	ISLDT(s)	((s) & SEL_LDT)	/* is it local or global */
#define	IDXSEL(s)	(((s) >> 3) & 0x1fff)		/* index of selector */
#define	LSEL(s,r)	(((s) << 3) | r | SEL_LDT)	/* a local selector */
#define	GSEL(s,r)	(((s) << 3) | r)		/* a global selector */

#ifndef LOCORE

/*
 * User segment descriptors (%cs, %ds etc for compatibility apps. 64 bit wide)
 * For long-mode apps, %cs only has the conforming bit in sd_type, the sd_dpl,
 * sd_p, sd_l and sd_def32 which must be zero).  %ds only has sd_p.
 */
struct	user_segment_descriptor {
	u_int64_t sd_lolimit:16;	/* segment extent (lsb) */
	u_int64_t sd_lobase:24;		/* segment base address (lsb) */
	u_int64_t sd_type:5;		/* segment type */
	u_int64_t sd_dpl:2;		/* segment descriptor priority level */
	u_int64_t sd_p:1;		/* segment descriptor present */
	u_int64_t sd_hilimit:4;		/* segment extent (msb) */
	u_int64_t sd_xx:1;		/* unused */
	u_int64_t sd_long:1;		/* long mode (cs only) */
	u_int64_t sd_def32:1;		/* default 32 vs 16 bit size */
	u_int64_t sd_gran:1;		/* limit granularity (byte/page units)*/
	u_int64_t sd_hibase:8;		/* segment base address  (msb) */
} __packed;

/*
 * System segment descriptors (128 bit wide)
 */
struct	system_segment_descriptor {
	u_int64_t sd_lolimit:16;	/* segment extent (lsb) */
	u_int64_t sd_lobase:24;		/* segment base address (lsb) */
	u_int64_t sd_type:5;		/* segment type */
	u_int64_t sd_dpl:2;		/* segment descriptor priority level */
	u_int64_t sd_p:1;		/* segment descriptor present */
	u_int64_t sd_hilimit:4;		/* segment extent (msb) */
	u_int64_t sd_xx0:3;		/* unused */
	u_int64_t sd_gran:1;		/* limit granularity (byte/page units)*/
	u_int64_t sd_hibase:40 __packed;/* segment base address  (msb) */
	u_int64_t sd_xx1:8;
	u_int64_t sd_mbz:5;		/* MUST be zero */
	u_int64_t sd_xx2:19;
} __packed;

/*
 * Gate descriptors (e.g. indirect descriptors, trap, interrupt etc. 128 bit)
 * Only interrupt and trap gates have gd_ist.
 */
struct	gate_descriptor {
	u_int64_t gd_looffset:16;	/* gate offset (lsb) */
	u_int64_t gd_selector:16;	/* gate segment selector */
	u_int64_t gd_ist:3;		/* IST table index */
	u_int64_t gd_xx:5;		/* unused */
	u_int64_t gd_type:5;		/* segment type */
	u_int64_t gd_dpl:2;		/* segment descriptor priority level */
	u_int64_t gd_p:1;		/* segment descriptor present */
	u_int64_t gd_hioffset:48 __packed;	/* gate offset (msb) */
	u_int64_t gd_xx1:32;
} __packed;

#endif /* !LOCORE */

/* system segments and gate types */
#define	SDT_SYSNULL	 0	/* system null */
#define	SDT_SYS286TSS	 1	/* system 286 TSS available */
#define	SDT_SYSLDT	 2	/* system 64-bit local descriptor table */
#define	SDT_SYS286BSY	 3	/* system 286 TSS busy */
#define	SDT_SYS286CGT	 4	/* system 286 call gate */
#define	SDT_SYSTASKGT	 5	/* system task gate */
#define	SDT_SYS286IGT	 6	/* system 286 interrupt gate */
#define	SDT_SYS286TGT	 7	/* system 286 trap gate */
#define	SDT_SYSNULL2	 8	/* system null again */
#define	SDT_SYSTSS	 9	/* system 64-bit TSS available */
#define	SDT_SYSNULL3	10	/* system null again */
#define	SDT_SYSBSY	11	/* system 64-bit TSS busy */
#define	SDT_SYSCGT	12	/* system 64-bit call gate */
#define	SDT_SYSNULL4	13	/* system null again */
#define	SDT_SYSIGT	14	/* system 64-bit interrupt gate */
#define	SDT_SYSTGT	15	/* system 64-bit trap gate */

/* memory segment types */
#define	SDT_MEMRO	16	/* memory read only */
#define	SDT_MEMROA	17	/* memory read only accessed */
#define	SDT_MEMRW	18	/* memory read write */
#define	SDT_MEMRWA	19	/* memory read write accessed */
#define	SDT_MEMROD	20	/* memory read only expand dwn limit */
#define	SDT_MEMRODA	21	/* memory read only expand dwn limit accessed */
#define	SDT_MEMRWD	22	/* memory read write expand dwn limit */
#define	SDT_MEMRWDA	23	/* memory read write expand dwn limit accessed */
#define	SDT_MEME	24	/* memory execute only */
#define	SDT_MEMEA	25	/* memory execute only accessed */
#define	SDT_MEMER	26	/* memory execute read */
#define	SDT_MEMERA	27	/* memory execute read accessed */
#define	SDT_MEMEC	28	/* memory execute only conforming */
#define	SDT_MEMEAC	29	/* memory execute only accessed conforming */
#define	SDT_MEMERC	30	/* memory execute read conforming */
#define	SDT_MEMERAC	31	/* memory execute read accessed conforming */

#ifndef LOCORE

struct savetls {
	struct tls_info info[2];
};

/*
 * Software definitions are in this convenient format,
 * which are translated into inconvenient segment descriptors
 * when needed to be used by the x86 hardware.
 */

struct	soft_segment_descriptor {
	uint64_t ssd_base;		/* segment base address  */
	uint64_t ssd_limit;	/* segment extent */
	uint64_t ssd_type:5;	/* segment type */
	uint64_t ssd_dpl:2;	/* segment descriptor priority level */
	uint64_t ssd_p:1;		/* segment descriptor present */
	uint64_t ssd_long:1;	/* long mode (for %cs) */
	uint64_t ssd_def32:1;	/* default 32 vs 16 bit size */
	uint64_t ssd_gran:1;	/* limit granularity (byte/page units)*/
} __packed;

/*
 * region descriptors, used to load gdt/idt tables before segments yet exist.
 */
struct region_descriptor {
	uint64_t rd_limit:16;		/* segment extent */
	uint64_t rd_base:64 __packed;	/* base address  */
} __packed;

#endif /* !LOCORE */

/*
 * Segment Protection Exception code bits
 */

#define	SEGEX_EXT	0x01	/* recursive or externally induced */
#define	SEGEX_IDT	0x02	/* interrupt descriptor table */
#define	SEGEX_TI	0x04	/* local descriptor table */
				/* other bits are affected descriptor index */
#define	SEGEX_IDX(s)	(((s) >> 3) & 0x1fff)

/*
 * Size of the IDT table.
 */
#define	NIDT	256		/* we use them all */
/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define	IDT_DE		0	/* #DE: Divide Error */
#define	IDT_DB		1	/* #DB: Debug */
#define	IDT_NMI		2	/* Nonmaskable External Interrupt */
#define	IDT_BP		3	/* #BP: Breakpoint */
#define	IDT_OF		4	/* #OF: Overflow */
#define	IDT_BR		5	/* #BR: Bound Range Exceeded */
#define	IDT_UD		6	/* #UD: Undefined/Invalid Opcode */
#define	IDT_NM		7	/* #NM: No Math Coprocessor */
#define	IDT_DF		8	/* #DF: Double Fault */
#define	IDT_FPUGP	9	/* Coprocessor Segment Overrun */
#define	IDT_TS		10	/* #TS: Invalid TSS */
#define	IDT_NP		11	/* #NP: Segment Not Present */
#define	IDT_SS		12	/* #SS: Stack Segment Fault */
#define	IDT_GP		13	/* #GP: General Protection Fault */
#define	IDT_PF		14	/* #PF: Page Fault */
#define	IDT_MF		16	/* #MF: FPU Floating-Point Error */
#define	IDT_AC		17	/* #AC: Alignment Check */
#define	IDT_MC		18	/* #MC: Machine Check */
#define	IDT_XF		19	/* #XF: SIMD Floating-Point Exception */
#define	IDT_IO_INTS	NRSVIDT	/* Base of IDT entries for I/O interrupts. */
#define	IDT_SYSCALL	0x80	/* System Call Interrupt Vector */

/*
 * Entries in the Global Descriptor Table (GDT)
 *
 * NOTE 1: Due to quirks in Intel's VMX implementation, the GDT
 *	   limit is set to 0xFFFF (representing 65536 bytes) on
 *	   VM exit.  Rather then have to do a heavy-weight reload,
 *	   we just maximally-size the table.
 */
#define	GNULL_SEL	0	/* Null Descriptor */
#define	GCODE_SEL	1	/* Kernel Code Descriptor */
#define	GDATA_SEL	2	/* Kernel Data Descriptor */
#define	GUCODE32_SEL	3	/* User 32 bit Code Descriptor */
#define	GUDATA_SEL	4	/* User 32/64 bit Data Descriptor */
#define	GUCODE_SEL	5	/* User 64 bit Code Descriptor */
#define	GPROC0_SEL	6	/* TSS for entering kernel etc */
/* slot 7 is second half of GPROC0_SEL */
#define	GUGS32_SEL	8	/* User 32 bit GS Descriptor */
#define	NGDT		9
#define MAXGDT_LIMIT	65536	/* (see note 1 above) */
#define MAXGDT_COUNT	(MAXGDT_LIMIT / sizeof(struct user_segment_descriptor))

#ifndef LOCORE

#ifdef _KERNEL
extern struct soft_segment_descriptor gdt_segs[];
extern struct gate_descriptor idt_arr[MAXCPU][NIDT];
extern struct region_descriptor r_idt_arr[];
extern struct region_descriptor r_gdt;
extern struct user_segment_descriptor gdt_cpu0[MAXGDT_COUNT];

void	lgdt(struct region_descriptor *rdp);
void	sdtossd(struct user_segment_descriptor *sdp,
	    struct soft_segment_descriptor *ssdp);
void	ssdtosd(struct soft_segment_descriptor *ssdp,
	    struct user_segment_descriptor *sdp);
void	ssdtosyssd(struct soft_segment_descriptor *ssdp,
	    struct system_segment_descriptor *sdp);
#endif /* _KERNEL */

#endif /* !LOCORE */

#endif /* !_CPU_SEGMENTS_H_ */
