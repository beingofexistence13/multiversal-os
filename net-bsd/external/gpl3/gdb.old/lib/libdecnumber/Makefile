#	$NetBSD: Makefile,v 1.10 2023/10/07 12:15:52 rin Exp $

.include <bsd.own.mk>
.include "../Makefile.inc"

LIBISPRIVATE=	yes

LIB=		decnumber

.include "${.CURDIR}/arch/${GDB_MACHINE_ARCH}/defs.mk"

GCPPFLAGS=	${G_archdefs} ${G_DEFS} ${G_INCLUDES} ${G_TDEFAULTS}
CPPFLAGS+=	-I${.CURDIR}/arch/${GDB_MACHINE_ARCH} \
		-I${.CURDIR}/../libbfd/arch/${GDB_MACHINE_ARCH} \
		-I${DIST}/include -I. \
		-I${DIST}/libdecnumber ${GCPPFLAGS:M-D*} \
		${GCPPFLAGS:M-I*:N-I.*}

SRCS=		${G_OBJS:.o=.c} ${G_SOURCES}

.PATH: ${DIST}/libdecnumber ${DIST}/libdecnumber/dpd

.include <bsd.lib.mk>
