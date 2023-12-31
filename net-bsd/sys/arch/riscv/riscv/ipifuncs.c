/*	$NetBSD: ipifuncs.c,v 1.2 2023/09/03 08:48:20 skrll Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ipifuncs.c,v 1.2 2023/09/03 08:48:20 skrll Exp $");

#include <sys/param.h>

#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/ipi.h>
#include <sys/xcall.h>

#include <uvm/uvm_extern.h>
#include <uvm/pmap/pmap_synci.h>
#include <uvm/pmap/pmap_tlb.h>

static void ipi_halt(void) __dead;

static const char * const ipi_names[] = {
	[IPI_NOP]	= "ipi nop",
	[IPI_AST]	= "ipi ast",
	[IPI_KPREEMPT]	= "ipi kpreempt",
	[IPI_SUSPEND]	= "ipi suspend",
	[IPI_HALT]	= "ipi halt",
	[IPI_XCALL]	= "ipi xcall",
	[IPI_GENERIC]	= "ipi generic",
};

static void
ipi_nop(struct cpu_info *ci)
{
	/*
	 * This is just a reason to get an interrupt so we get
	 * kicked out of cpu_idle().
	 */
}

static void
ipi_ast(struct cpu_info *ci)
{
	ci->ci_onproc->l_md.md_astpending = 1;
}

#ifdef __HAVE_PREEMPTION
static inline void
ipi_kpreempt(struct cpu_info *ci)
{
	softint_trigger(SOFTINT_KPREEMPT);
}
#endif

/*
 * Process cpu stop-self event.
 * XXX could maybe add/use locoresw halt function?
 */
static void
ipi_halt(void)
{
	const u_int my_cpu = cpu_index(curcpu());
	printf("cpu%u: shutting down\n", my_cpu);
	kcpuset_set(cpus_halted, my_cpu);
	splhigh();
	for (;;)
		;
	/* NOTREACHED */
}

void
ipi_process(struct cpu_info *ci, unsigned long ipi_mask)
{
	KASSERT(cpu_intr_p());

	if (ipi_mask & __BIT(IPI_NOP)) {
		ci->ci_evcnt_per_ipi[IPI_NOP].ev_count++;
		ipi_nop(ci);
	}
	if (ipi_mask & __BIT(IPI_AST)) {
		ci->ci_evcnt_per_ipi[IPI_AST].ev_count++;
		ipi_ast(ci);
	}
	if (ipi_mask & __BIT(IPI_SUSPEND)) {
		ci->ci_evcnt_per_ipi[IPI_SUSPEND].ev_count++;
		cpu_pause();
	}
	if (ipi_mask & __BIT(IPI_HALT)) {
		ci->ci_evcnt_per_ipi[IPI_HALT].ev_count++;
		ipi_halt();
	}
	if (ipi_mask & __BIT(IPI_XCALL)) {
		ci->ci_evcnt_per_ipi[IPI_XCALL].ev_count++;
		xc_ipi_handler();
	}
	if (ipi_mask & __BIT(IPI_GENERIC)) {
		ci->ci_evcnt_per_ipi[IPI_GENERIC].ev_count++;
		ipi_cpu_handler();
	}
#ifdef __HAVE_PREEMPTION
	if (ipi_mask & __BIT(IPI_KPREEMPT)) {
		ci->ci_evcnt_per_ipi[IPI_KPREEMPT].ev_count++;
		ipi_kpreempt(ci);
	}
#endif
}

void
ipi_init(struct cpu_info *ci)
{
	evcnt_attach_dynamic(&ci->ci_evcnt_all_ipis, EVCNT_TYPE_INTR,
	    NULL, device_xname(ci->ci_dev), "ipi");

	for (size_t i = 0; i < NIPIS; i++) {
		KASSERTMSG(ipi_names[i] != NULL, "%zu", i);
		evcnt_attach_dynamic(&ci->ci_evcnt_per_ipi[i], EVCNT_TYPE_INTR,
		    NULL, device_xname(ci->ci_dev), ipi_names[i]);
	}
}
