# $FreeBSD: head/lib/libfetch/Makefile 298107 2016-04-16 07:45:30Z gjb $

LIB=		fetch
CFLAGS+=	-I.
CFLAGS+=	-DINET6 -DFTP_COMBINE_CWDS
SRCS=		fetch.c common.c ftp.c http.c file.c \
		ftperr.h httperr.h
INCS=		fetch.h
MAN=		fetch.3
CLEANFILES=	ftperr.h httperr.h

CFLAGS+=	-DWITH_SSL
DPADD=		${LIBSSL} ${LIBCRYPTO}
LDADD=		-lprivate_ssl -lprivate_crypto
CFLAGS+=	${PRIVATELIB_CFLAGS}
LDFLAGS+=	${PRIVATELIB_LDFLAGS}

SHLIB_MAJOR=    4

ftperr.h: ftp.errors ${.CURDIR}/Makefile
	@echo "static struct fetcherr ftp_errlist[] = {" > ${.TARGET}
	@cat ${.CURDIR}/ftp.errors \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

httperr.h: http.errors ${.CURDIR}/Makefile
	@echo "static struct fetcherr http_errlist[] = {" > ${.TARGET}
	@cat ${.CURDIR}/http.errors \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

MLINKS+= fetch.3 fetchFreeURL.3
MLINKS+= fetch.3 fetchGet.3
MLINKS+= fetch.3 fetchGetFTP.3
MLINKS+= fetch.3 fetchGetFile.3
MLINKS+= fetch.3 fetchGetHTTP.3
MLINKS+= fetch.3 fetchGetURL.3
MLINKS+= fetch.3 fetchList.3
MLINKS+= fetch.3 fetchListFTP.3
MLINKS+= fetch.3 fetchListFile.3
MLINKS+= fetch.3 fetchListHTTP.3
MLINKS+= fetch.3 fetchListURL.3
MLINKS+= fetch.3 fetchMakeURL.3
MLINKS+= fetch.3 fetchParseURL.3
MLINKS+= fetch.3 fetchPut.3
MLINKS+= fetch.3 fetchPutFTP.3
MLINKS+= fetch.3 fetchPutFile.3
MLINKS+= fetch.3 fetchPutHTTP.3
MLINKS+= fetch.3 fetchPutURL.3
MLINKS+= fetch.3 fetchReqHTTP.3
MLINKS+= fetch.3 fetchStat.3
MLINKS+= fetch.3 fetchStatFTP.3
MLINKS+= fetch.3 fetchStatFile.3
MLINKS+= fetch.3 fetchStatHTTP.3
MLINKS+= fetch.3 fetchStatURL.3
MLINKS+= fetch.3 fetchXGet.3
MLINKS+= fetch.3 fetchXGetFTP.3
MLINKS+= fetch.3 fetchXGetFile.3
MLINKS+= fetch.3 fetchXGetHTTP.3
MLINKS+= fetch.3 fetchXGetURL.3

.include <bsd.lib.mk>
