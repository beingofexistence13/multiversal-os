#	$NetBSD: Makefile,v 1.8 2023/09/05 22:14:08 mrg Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/sys/crypto/chacha

TESTS_C=	t_chacha

AFLAGS+=	-D_LOCORE

.PATH:	${NETBSDSRCDIR}/sys/crypto/chacha
CPPFLAGS+=	-I${NETBSDSRCDIR}/sys

SRCS.t_chacha+=	t_chacha.c

SRCS.t_chacha+=	chacha_ref.c
SRCS.t_chacha+=	chacha_selftest.c

.if !empty(MACHINE_ARCH:Mearmv7*) || !empty(MACHINE_ARCH:Maarch64*)

.PATH:	${NETBSDSRCDIR}/sys/crypto/chacha/arch/arm
CPPFLAGS+=	-I${NETBSDSRCDIR}/sys/crypto/chacha/arch/arm

CLANG_NO_NONPORTABLE_VECTOR_INITIALIZATION= ${${ACTIVE_CC} == "clang":? -Wno-nonportable-vector-initialization :}

SRCS.t_chacha+=	chacha_neon.c
COPTS.chacha_neon.c+=	${CLANG_NO_NONPORTABLE_VECTOR_INITIALIZATION}
.if !empty(MACHINE_ARCH:Mearmv7*)
SRCS.t_chacha+=	chacha_neon_32.S
.elif !empty(MACHINE_ARCH:Maarch64*)
SRCS.t_chacha+=	chacha_neon_64.S
.endif
SRCS.t_chacha+=	chacha_neon_impl.c

.if !empty(MACHINE_ARCH:Mearmv7*) && empty(MACHINE_ARCH:Mearmv7hf*)
COPTS.chacha_neon.c+=		-mfloat-abi=softfp -mfpu=neon
AOPTS.chacha_neon_32.S+=	-D__SOFTFP__
.endif

.endif				# earmv7 or aarch64

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"

.PATH:	${NETBSDSRCDIR}/sys/crypto/chacha/arch/x86
CPPFLAGS+=	-I${NETBSDSRCDIR}/sys/crypto/chacha/arch/x86

SRCS.t_chacha+=	chacha_sse2.c
SRCS.t_chacha+=	chacha_sse2_impl.c
COPTS.chacha_sse2.c+=	-msse -msse2
# XXXGCC12 - believes subkey[] is maybe uninitialised, but it is not.
COPTS.chacha_sse2.c+=	${${ACTIVE_CC} == "gcc":? -Wno-maybe-uninitialized :}

.endif				# x86

WARNS=		5

.include <bsd.test.mk>
