/*
 * (MPSAFE)
 *
 * Copyright (C) 2003,2004
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 * $FreeBSD: src/sys/dev/dcons/dcons_os.c,v 1.4 2004/10/24 12:41:04 simokawa Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>
#include <sys/ttydefaults.h>	/* for TTYDEF_* */
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/caps.h>
#include <sys/ucred.h>
#include <sys/bus.h>

#include "dcons.h"
#include "dcons_os.h"

#include <ddb/ddb.h>
#include <sys/reboot.h>

#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "opt_ddb.h"
#include "opt_comconsole.h"
#include "opt_dcons.h"

#ifndef DCONS_POLL_HZ
#define DCONS_POLL_HZ	100
#endif

#ifndef DCONS_BUF_SIZE
#define DCONS_BUF_SIZE (16*1024)
#endif

#ifndef DCONS_FORCE_CONSOLE
#define DCONS_FORCE_CONSOLE	0	/* Mostly for FreeBSD-4/DragonFly */
#endif

#ifndef DCONS_FORCE_GDB
#define DCONS_FORCE_GDB	1
#endif

static d_open_t		dcons_open;
static d_close_t	dcons_close;
static d_ioctl_t	dcons_ioctl;

static struct dev_ops dcons_ops = {
	{ "dcons", 0, D_TTY },
	.d_open =	dcons_open,
	.d_close =	dcons_close,
	.d_read =	ttyread,
	.d_write =	ttywrite,
	.d_ioctl =	dcons_ioctl,
	.d_kqfilter =	ttykqfilter,
	.d_revoke =	ttyrevoke
};

#ifndef KLD_MODULE
static char bssbuf[DCONS_BUF_SIZE];	/* buf in bss */
#endif

/* global data */
static struct dcons_global dg;
struct dcons_global *dcons_conf;
static int poll_hz = DCONS_POLL_HZ;

static struct dcons_softc sc[DCONS_NPORT];

SYSCTL_NODE(_kern, OID_AUTO, dcons, CTLFLAG_RD, 0, "Dumb Console");
SYSCTL_INT(_kern_dcons, OID_AUTO, poll_hz, CTLFLAG_RW, &poll_hz, 0,
				"dcons polling rate");

static int drv_init = 0;
static struct callout dcons_callout;
struct dcons_buf *dcons_buf;		/* for local dconschat */

#define DEV	cdev_t
#define THREAD	d_thread_t

static void	dcons_tty_start(struct tty *);
static int	dcons_tty_param(struct tty *, struct termios *);
static void	dcons_timeout(void *);
static int	dcons_drv_init(int);

static cn_probe_t	dcons_cnprobe;
static cn_init_t	dcons_cninit;
static cn_init_fini_t	dcons_cninit_fini;
static cn_getc_t	dcons_cngetc;
static cn_checkc_t 	dcons_cncheckc;
static cn_putc_t	dcons_cnputc;

CONS_DRIVER(dcons, dcons_cnprobe, dcons_cninit, dcons_cninit_fini,
	    NULL, dcons_cngetc, dcons_cncheckc, dcons_cnputc, NULL, NULL);

#if defined(DDB) && defined(ALT_BREAK_TO_DEBUGGER)
static int
dcons_check_break(struct dcons_softc *dc, int c)
{
	if (c < 0)
		return (c);

	switch (dc->brk_state) {
	case STATE1:
		if (c == KEY_TILDE)
			dc->brk_state = STATE2;
		else
			dc->brk_state = STATE0;
		break;
	case STATE2:
		dc->brk_state = STATE0;
		if (c == KEY_CTRLB) {
#if DCONS_FORCE_GDB
			if (dc->flags & DC_GDB)
				boothowto |= RB_GDB;
#endif
			breakpoint();
		}
	}
	if (c == KEY_CR)
		dc->brk_state = STATE1;
	return (c);
}
#else
#define	dcons_check_break(dc, c)	(c)
#endif

static int
dcons_os_checkc(struct dcons_softc *dc)
{
	int c;

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_POSTREAD);
  
	c = dcons_check_break(dc, dcons_checkc(dc));

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_PREREAD);

	return (c);
}

static int
dcons_os_getc(struct dcons_softc *dc)
{
	int c;

	while ((c = dcons_os_checkc(dc)) == -1);

	return (c & 0xff);
} 

static void
dcons_os_putc(struct dcons_softc *dc, int c)
{
	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_POSTWRITE);

	dcons_putc(dc, c);

	if (dg.dma_tag != NULL)
		bus_dmamap_sync(dg.dma_tag, dg.dma_map, BUS_DMASYNC_PREWRITE);
}
static int
dcons_open(struct dev_open_args *ap)
{
	cdev_t dev = ap->a_head.a_dev;
	struct tty *tp;
	int unit, error;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = ttymalloc(&dev->si_tty);
	lwkt_gettoken(&tp->t_token);
	tp->t_oproc = dcons_tty_start;
	tp->t_param = dcons_tty_param;
	tp->t_stop = nottystop;
	tp->t_dev = dev;

	error = 0;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) &&
		   caps_priv_check(ap->a_cred, SYSCAP_RESTRICTEDROOT))
	{
		lwkt_reltoken(&tp->t_token);

		return (EBUSY);
	}

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	lwkt_reltoken(&tp->t_token);

	return (error);
}

static int
dcons_close(struct dev_close_args *ap)
{
	cdev_t dev = ap->a_head.a_dev;
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	lwkt_gettoken(&tp->t_token);
	if (tp->t_state & TS_ISOPEN) {
		(*linesw[tp->t_line].l_close)(tp, ap->a_fflag);
		ttyclose(tp);
	}
	lwkt_reltoken(&tp->t_token);

	return (0);
}

static int
dcons_ioctl(struct dev_ioctl_args *ap)
{
	cdev_t dev = ap->a_head.a_dev;
	int	unit;
	struct	tty *tp;
	int	error;

	unit = minor(dev);
	if (unit != 0)
		return (ENXIO);

	tp = dev->si_tty;
	lwkt_gettoken(&tp->t_token);
	error = (*linesw[tp->t_line].l_ioctl)(tp, ap->a_cmd, ap->a_data, ap->a_fflag, ap->a_cred);
	if (error != ENOIOCTL) {
		lwkt_reltoken(&tp->t_token);
		return (error);
	}

	error = ttioctl(tp, ap->a_cmd, ap->a_data, ap->a_fflag);
	if (error != ENOIOCTL) {
		lwkt_reltoken(&tp->t_token);
		return (error);
	}

	lwkt_reltoken(&tp->t_token);
	return (ENOTTY);
}

static int
dcons_tty_param(struct tty *tp, struct termios *t)
{
	lwkt_gettoken(&tp->t_token);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	lwkt_reltoken(&tp->t_token);
	return 0;
}

static void
dcons_tty_start(struct tty *tp)
{
	struct dcons_softc *dc;

	lwkt_gettoken(&tp->t_token);
	dc = (struct dcons_softc *)tp->t_dev->si_drv1;
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		lwkt_reltoken(&tp->t_token);
		return;
	}

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		dcons_os_putc(dc, clist_getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	lwkt_reltoken(&tp->t_token);
}

static void
dcons_timeout(void *dummy __unused)
{
	struct tty *tp;
	struct dcons_softc *dc;
	int i, c, polltime;

	for (i = 0; i < DCONS_NPORT; i ++) {
		dc = &sc[i];
		tp = ((DEV)dc->dev)->si_tty;
		lwkt_gettoken(&tp->t_token);
		while ((c = dcons_os_checkc(dc)) != -1) {
			if (tp->t_state & TS_ISOPEN)
				(*linesw[tp->t_line].l_rint)(c, tp);
		}
		lwkt_reltoken(&tp->t_token);
	}
	polltime = hz / poll_hz;
	if (polltime < 1)
		polltime = 1;
	callout_reset(&dcons_callout, polltime, dcons_timeout, NULL);
}

static void
dcons_cnprobe(struct consdev *cp)
{
	cp->cn_probegood = 1;
#if DCONS_FORCE_CONSOLE
	cp->cn_pri = CN_REMOTE;
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

static void
dcons_cninit(struct consdev *cp)
{
	dcons_drv_init(0);
#ifdef CONS_NODEV
	cp->cn_arg
#else
	cp->cn_private
#endif
		= (void *)&sc[DCONS_CON]; /* share port0 with unit0 */
}

static void
dcons_cninit_fini(struct consdev *cp)
{
	cp->cn_dev = make_dev(&dcons_ops, DCONS_CON,
			      UID_ROOT, GID_WHEEL, 0600, "dcons");
}

#ifdef CONS_NODEV
static int
dcons_cngetc(struct consdev *cp)
{
	struct dcons_softc *dc = (struct dcons_softc *)cp->cn_arg;
	return (dcons_os_getc(dc));
}
static int
dcons_cncheckc(struct consdev *cp)
{
	struct dcons_softc *dc = (struct dcons_softc *)cp->cn_arg;
	return (dcons_os_checkc(dc));
}
static void
dcons_cnputc(struct consdev *cp, int c)
{
	struct dcons_softc *dc = (struct dcons_softc *)cp->cn_arg;
	dcons_os_putc(dc, c);
}
#else
static int
dcons_cngetc(void *private)
{
	struct dcons_softc *dc = (struct dcons_softc *)private;
	return (dcons_os_getc(dc));
}
static int
dcons_cncheckc(void *private)
{
	struct dcons_softc *dc = (struct dcons_softc *)private;
	return (dcons_os_checkc(dc));
}
static void
dcons_cnputc(void *private, int c)
{
	struct dcons_softc *dc = (struct dcons_softc *)private;
	dcons_os_putc(dc, c);
}
#endif

static int
dcons_drv_init(int stage)
{
#ifdef __i386__
	quad_t addr, size;
#endif

	if (drv_init)
		return(drv_init);

	drv_init = -1;

	bzero(&dg, sizeof(dg));
	dcons_conf = &dg;
	dg.cdev = &dcons_consdev;
	dg.buf = NULL;
	dg.size = DCONS_BUF_SIZE;

#ifdef __i386__
	if (kgetenv_quad("dcons.addr", &addr) > 0 &&
	    kgetenv_quad("dcons.size", &size) > 0) {
		vm_paddr_t pa;
		/*
		 * Allow read/write access to dcons buffer.
		 */
		for (pa = trunc_page(addr); pa < addr + size; pa += PAGE_SIZE)
			pmap_kmodify_rw((vm_offset_t)(KERNBASE + pa));
		cpu_invltlb();
		/* XXX P to V */
		dg.buf = (struct dcons_buf *)(vm_offset_t)(KERNBASE + addr);
		dg.size = size;
		if (dcons_load_buffer(dg.buf, dg.size, sc) < 0)
			dg.buf = NULL;
	}
#endif
	if (dg.buf != NULL)
		goto ok;

#ifndef KLD_MODULE
	if (stage == 0) { /* XXX or cold */
		/*
		 * DCONS_FORCE_CONSOLE == 1 and statically linked.
		 * called from cninit(). can't use contigmalloc yet .
		 */
		dg.buf = (struct dcons_buf *) bssbuf;
		dcons_init(dg.buf, dg.size, sc);
	} else
#endif
	{
		/*
		 * DCONS_FORCE_CONSOLE == 0 or kernel module case.
		 * if the module is loaded after boot,
		 * bssbuf could be non-continuous.
		 */ 
		dg.buf = (struct dcons_buf *) contigmalloc(dg.size,
			M_DEVBUF, 0, 0x10000, 0xffffffff, PAGE_SIZE, 0ul);
		dcons_init(dg.buf, dg.size, sc);
	}

ok:
	dcons_buf = dg.buf;

#if defined(DDB) && DCONS_FORCE_GDB
	if (gdb_tab == NULL) {
		gdb_tab = &dcons_consdev;
		dcons_consdev.cn_gdbprivate = &sc[DCONS_GDB];
	}
#endif
	drv_init = 1;

	return 0;
}

static int
dcons_attach_port(int port, char *name, int flags)
{
	struct dcons_softc *dc;
	struct tty *tp;
	DEV dev;

	dc = &sc[port];
	dc->flags = flags;
	dev = make_dev(&dcons_ops, port, UID_ROOT, GID_WHEEL, 0600,
		       "%s", name);
	dc->dev = (void *)dev;
	dev->si_drv1 = (void *)dc;

	tp = ttymalloc(&dev->si_tty);
	tp->t_oproc = dcons_tty_start;
	tp->t_param = dcons_tty_param;
	tp->t_stop = nottystop;
	tp->t_dev = dc->dev;

	return(0);
}

static int
dcons_attach(void)
{
	int polltime;

	dcons_attach_port(DCONS_CON, "dcons", 0);
	dcons_attach_port(DCONS_GDB, "dgdb", DC_GDB);
	callout_init_mp(&dcons_callout);
	polltime = hz / poll_hz;
	if (polltime < 1)
		polltime = 1;
	callout_reset(&dcons_callout, polltime, dcons_timeout, NULL);
	return(0);
}

static int
dcons_detach(int port)
{
	struct	tty *tp;
	struct dcons_softc *dc;

	dc = &sc[port];

	tp = ((DEV)dc->dev)->si_tty;
	lwkt_gettoken(&tp->t_token);

	if (tp->t_state & TS_ISOPEN) {
		kprintf("dcons: still opened\n");
		(*linesw[tp->t_line].l_close)(tp, 0);
		ttyclose(tp);
	}
	/* XXX
	 * must wait until all device are closed.
	 */
#ifdef __DragonFly__
	tsleep((void *)dc, 0, "dcodtc", hz/4);
#else
	tsleep((void *)dc, PWAIT, "dcodtc", hz/4);
#endif
	lwkt_reltoken(&tp->t_token);
	destroy_dev(dc->dev);

	return(0);
}


/* cnXXX works only for FreeBSD-5 */
static int
dcons_modevent(module_t mode, int type, void *data)
{
	int err = 0, ret;

	switch (type) {
	case MOD_LOAD:
		ret = dcons_drv_init(1);
		dcons_attach();
#if 0 /* XXX __FreeBSD_version >= 500000 */
		if (ret == 0) {
			dcons_cnprobe(&dcons_consdev);
			dcons_cninit(&dcons_consdev);
			cnadd(&dcons_consdev);
		}
#endif
		break;
	case MOD_UNLOAD:
		kprintf("dcons: unload\n");
		callout_stop(&dcons_callout);
#if defined(DDB) && DCONS_FORCE_GDB
#ifdef CONS_NODEV
		gdb_arg = NULL;
#else
		if (gdb_tab == &dcons_consdev)
			gdb_tab = NULL;
#endif
#endif
#if 0 /* XXX __FreeBSD_version >= 500000 */
		cnremove(&dcons_consdev);
#endif
		dcons_detach(DCONS_CON);
		dcons_detach(DCONS_GDB);
		dg.buf->magic = 0;

		contigfree(dg.buf, DCONS_BUF_SIZE, M_DEVBUF);

		break;
	case MOD_SHUTDOWN:
		dg.buf->magic = 0;
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return(err);
}

DEV_MODULE(dcons, dcons_modevent, NULL);
MODULE_VERSION(dcons, DCONS_VERSION);
