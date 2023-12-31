#	$NetBSD: Makefile,v 1.15 2023/08/11 06:52:45 mrg Exp $

HOSTPROG_CXX=	1
LIBISPRIVATE=	yes

# For ../Makefile.inc and bsd.own.mk
.include <bsd.init.mk>

LIB=		common-target

.include "${.CURDIR}/arch/${GCC_MACHINE_ARCH}.mk"
SRCS=		${G_OBJS-libcommon-target:S/common\///:.o=.cc}

CPPFLAGS+=	-I${GCCARCH} -I${BACKENDOBJ} -I. \
		${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*} \
		-DPREFIX=\"/usr\" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\" \
		-DCONFIGURE_SPECS="\"\"" \
		-I${BACKENDOBJ}

#COPTS.prefix.cc+=	-Wno-stack-protector
#COPTS.opts-common.cc+=	-Wno-stack-protector
#COPTS.rs6000-common.cc+=	-Wno-stack-protector
#COPTS.arm-common.cc+=	-Wno-stack-protector
.if ${MACHINE_CPU} == "sh3"
COPTS.opts.cc+=		${CC_WNO_FORMAT_TRUNCATION}
.endif

GENPROG_ERROR_DEPENDS=errors.lo

.include "../Makefile.options"
.include "../../Makefile.hooks"

${SRCS}: ${GCCARCH}/defs.mk

${G_common_out_file:T:R}.o:  common/common-target-hooks-def.h

DPSRCS+=	options.h

HOST_CPPFLAGS+=	-I${BACKENDOBJ} -I${GCCARCH} -I. -I${DIST}/include -I${DIST}/gcc
HOST_CPPFLAGS+=	-DGENERATOR_FILE

MKPIC:=		no
MKPICLIB:=	no

.include <bsd.lib.mk>

# Force using C++ for this
HOST_CC:=	${HOST_CXX}
HOST_CFLAGS:=	${HOST_CXXFLAGS}
CC:=		${CXX}
CFLAGS:=	${CXXFLAGS}

.PATH: ${DIST}/gcc ${DIST}/gcc/common
.PATH: ${G_common_out_file:H}
