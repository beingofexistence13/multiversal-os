# $FreeBSD: src/usr.bin/awk/Makefile,v 1.9.2.1 2002/06/21 20:12:08 obrien Exp $

AWKSRC=	${.CURDIR}/../../../contrib/awk
.PATH: ${AWKSRC}

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c awkgram.tab.h
WARNS?=	2

CFLAGS+= -I. -I${AWKSRC} -DHAS_ISBLANK

DPADD=	${LIBM}
LDADD=	-lm

CLEANFILES= proctab.c awkgram.tab.h

awkgram.tab.h: awkgram.h
	${LN} -sf ${.ALLSRC} ${.TARGET}

proctab.c: awkgram.tab.h ${AWKSRC}/maketab.c
	${.OBJDIR}/../maketab/maketab.nx awkgram.tab.h > ${.OBJDIR}/proctab.c

.include <bsd.prog.mk>
