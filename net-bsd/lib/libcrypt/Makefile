#	$NetBSD: Makefile,v 1.36 2023/10/07 12:15:53 rin Exp $

USE_FORT?=	yes	# cryptographic software

.include <bsd.own.mk>

USE_SHLIBDIR=	yes

.if (defined(MKARGON2) && ${MKARGON2} != "no")
HAVE_ARGON2=1
.endif

LIB=	crypt

SRCS=	crypt.c md5crypt.c bcrypt.c crypt-sha1.c util.c pw_gensalt.c
SRCS+=	hmac_sha1.c

CPPFLAGS+= -include ${.CURDIR}/namespace.h

.if defined(HAVE_ARGON2)
ARGON2DIR=	${NETBSDSRCDIR}/external/apache2/argon2
.PATH: ${ARGON2DIR}/dist/phc-winner-argon2/src \
	${ARGON2DIR}/dist/phc-winner-argon2/src/blake2 \
	${ARGON2DIR}/dist/phc-winner-argon2/include
CPPFLAGS+=	-DHAVE_ARGON2 -DARGON2_NO_THREADS
CPPFLAGS+=	-I${ARGON2DIR}/dist/phc-winner-argon2/include
SRCS+=		crypt-argon2.c
# libargon2 sources
.  for src in argon2.c core.c blake2b.c encoding.c ref.c
SRCS+=		${src}
COPTS.${src}+=	-fvisibility=hidden
.  endfor
.endif

WARNS?=	5

MAN=	crypt.3 pw_gensalt.3
MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3

# XXXGCC12 - init_perm() takes a larger array than IE3264[] is, but
# it doesn't need more.
COPTS.crypt.c+=	${CC_WNO_STRINGOP_OVERFLOW}

.if make(*.test)
.SUFFIXES: .test
.c.test:
	${CC} -o $@ -DUNIT_TEST -DMAIN ${LDSTATIC} ${.IMPSRC} ${LDADD}
LDADD+= -L${.OBJDIR} -l${LIB} -L${.OBJDIR}/../../libc
DPADD+= lib${LIB}.a
LDSTATIC?= -static
.include <bsd.prog.mk>
.else
.include <bsd.lib.mk>
.endif
