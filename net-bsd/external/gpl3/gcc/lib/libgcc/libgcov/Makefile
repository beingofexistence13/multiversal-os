#	$NetBSD: Makefile,v 1.10 2023/08/01 05:57:56 mrg Exp $

REQUIRETOOLS=	yes
NOPIC=		# defined
NOPROFILE=	# defined
NOLINT=		# defined

LIB=		gcov

.include "../../../Makefile.gcc_path"

.include <bsd.own.mk>
.include <bsd.init.mk>

.if ${MKGCC} != "no"

.include "${.CURDIR}/arch/${LIBGCC_MACHINE_ARCH}/defs.mk"

LIBGCOVMERGESRCS=	${G_LIBGCOV_MERGE:=.c}
LIBGCOVPROFILERSRCS=	${G_LIBGCOV_PROFILER:=.c}
LIBGCOVINTERFACESRCS=	${G_LIBGCOV_INTERFACE:=.c}
LIBGCOVDRIVERSRCS=	${G_LIBGCOV_DRIVER:=.c}

LIBGCOVSRCS=	${LIBGCOVMERGESRCS} ${LIBGCOVPROFILERSRCS} \
		${LIBGCOVINTERFACESRCS} ${LIBGCOVDRIVERSRCS}

CPPFLAGS+=	-I${.CURDIR}/arch/${LIBGCC_MACHINE_ARCH}
CPPFLAGS+=	-Wno-stack-protector

DPSRCS+=	${.CURDIR}/arch/${LIBGCC_MACHINE_ARCH}/defs.mk
SRCS+=		${LIBGCOVSRCS}

${OBJS}:	version.h

CLEANFILES+=	${LIBGCOVSRCS}

${LIBGCOVMERGESRCS}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcov-merge.c>\n' >${.TARGET}

${LIBGCOVPROFILERSRCS}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcov-profiler.c>\n' >${.TARGET}

${LIBGCOVINTERFACESRCS}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcov-interface.c>\n' >${.TARGET}

${LIBGCOVDRIVERSRCS}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcov-driver.c>\n' >${.TARGET}

.include "../../../../lgpl3/gmp/Makefile.arch"
.include "../../../Makefile.version_h"
.include "../../../Makefile.autobuild_h"

DPSRCS+=	version.h

.include <bsd.lib.mk>
.else
.include <bsd.prog.mk> # do nothing
.endif
