.PATH: ${.CURDIR}/../libc/resolv

SRCS+=	h_errno.c herror.c mtctxres.c res_comp.c res_data.c res_debug.c \
	res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c \
	res_query.c res_send.c res_state.c res_update.c

CMAPS+=	${.CURDIR}/resolv/Symbol.map
