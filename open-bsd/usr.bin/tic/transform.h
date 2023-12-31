/*	$OpenBSD: transform.h,v 1.3 2023/10/17 09:52:10 nicm Exp $	*/

#ifndef __TRANSFORM_H
#define __TRANSFORM_H 1
#include <progs.priv.h>
extern bool same_program(const char *, const char *);
#define PROG_CAPTOINFO "captoinfo"
#define PROG_INFOTOCAP "infotocap"
#define PROG_CLEAR     "clear"
#define PROG_RESET     "reset"
#define PROG_INIT      "init"
#endif /* __TRANSFORM_H */
