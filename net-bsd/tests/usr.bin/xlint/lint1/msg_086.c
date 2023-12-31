/*	$NetBSD: msg_086.c,v 1.7 2023/08/02 18:51:25 rillig Exp $	*/
# 3 "msg_086.c"

// Test for message: automatic '%s' hides external declaration [86]

/* lint1-flags: -S -g -h -w -X 351 */

extern int identifier;

int
local_auto(void)
{
	/* expect+1: warning: automatic 'identifier' hides external declaration [86] */
	int identifier = 3;
	return identifier;
}

/* XXX: the function parameter does not trigger the warning. */
int
arg_auto(int identifier)
{
	return identifier;
}
