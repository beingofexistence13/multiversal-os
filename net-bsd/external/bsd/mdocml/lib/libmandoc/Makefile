# $NetBSD: Makefile,v 1.13 2023/10/07 12:15:50 rin Exp $

LIBISPRIVATE=	yes

LIB=	mandoc
SRCS=	\
arch.c \
att.c \
chars.c \
compat_ohash.c \
compat_recallocarray.c \
eqn.c \
lib.c \
man.c \
man_macro.c \
man_validate.c \
mandoc.c \
mandoc_aux.c \
mandoc_msg.c \
mandoc_ohash.c \
mandoc_xr.c \
mdoc.c \
mdoc_argv.c \
mdoc_macro.c \
mdoc_man.c \
mdoc_state.c \
mdoc_validate.c \
msec.c \
preconv.c \
read.c \
roff.c \
roff_validate.c \
st.c \
tag.c \
tbl.c \
tbl_data.c \
tbl_layout.c \
tbl_opts.c

MAN=	mandoc.3

COPTS.man_validate.c+=-Wno-error=array-bounds

.include <bsd.lib.mk>
