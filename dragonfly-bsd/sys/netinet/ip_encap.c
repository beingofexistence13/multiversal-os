/*	$FreeBSD: src/sys/netinet/ip_encap.c,v 1.1.2.5 2003/01/23 21:06:45 sam Exp $	*/
/*	$KAME: ip_encap.c,v 1.41 2001/03/15 08:35:08 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * My grandfather said that there's a devil inside tunnelling technology...
 *
 * We have surprisingly many protocols that want packets with IP protocol
 * #4 or #41.  Here's a list of protocols that want protocol #41:
 *	RFC1933 configured tunnel
 *	RFC1933 automatic tunnel
 *	RFC2401 IPsec tunnel
 *	RFC2473 IPv6 generic packet tunnelling
 *	RFC2529 6over4 tunnel
 *	mobile-ip6 (uses RFC2473)
 *	RFC3056 6to4 tunnel
 *	isatap tunnel
 * Here's a list of protocol that want protocol #4:
 *	RFC1853 IPv4-in-IPv4 tunnelling
 *	RFC2003 IPv4 encapsulation within IPv4
 *	RFC2344 reverse tunnelling for mobile-ip4
 *	RFC2401 IPsec tunnel
 * Well, what can I say.  They impose different en/decapsulation mechanism
 * from each other, so they need separate protocol handler.  The only one
 * we can easily determine by protocol # is IPsec, which always has
 * AH/ESP/IPComp header right after outer IP header.
 *
 * So, clearly good old protosw does not work for protocol #4 and #41.
 * The code will let you match protocol via src/dst address pair.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6protosw.h>
#endif

#include <machine/stdarg.h>

#include <net/net_osdep.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/thread2.h>
MALLOC_DEFINE(M_IPENCAP, "IP Encapsulation", "IP Encapsulation");

static void encap_add (struct encaptab *);
static int mask_match (const struct encaptab *, const struct sockaddr *,
		const struct sockaddr *);
static void encap_fillarg (struct mbuf *, const struct encaptab *);

#ifndef LIST_HEAD_INITIALIZER
/* rely upon BSS initialization */
LIST_HEAD(, encaptab) encaptab;
#else
LIST_HEAD(, encaptab) encaptab = LIST_HEAD_INITIALIZER(&encaptab);
#endif

int     (*ipip_input)(struct mbuf **, int *, int); /* hook for mrouting */

void
encap_init(void)
{
	static int initialized = 0;

	if (initialized)
		return;
	initialized++;
#if 0
	/*
	 * we cannot use LIST_INIT() here, since drivers may want to call
	 * encap_attach(), on driver attach.  encap_init() will be called
	 * on AF_INET{,6} initialization, which happens after driver
	 * initialization - using LIST_INIT() here can nuke encap_attach()
	 * from drivers.
	 */
	LIST_INIT(&encaptab);
#endif
}

#ifdef INET
int
encap4_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	int off = *offp;
	struct ip *ip;
	struct sockaddr_in s, d;
	const struct protosw *psw;
	struct encaptab *ep, *match;
	int prio, matchprio;

	if (!IN_NETISR(0)) {
		/*
		 * NOTE:
		 * Some NICs, noticeably igb(4) and ix(4), use inner IP
		 * datagram to calculate the packet hash, which leads us
		 * here.
		 */
		m->m_flags &= ~M_HASH;
		m = ip_rehashm(m);
		if (m != NULL) {
			lwkt_port_t port = netisr_hashport(m->m_pkthdr.hash);

			KASSERT(port != &curthread->td_msgport,
			    ("mbuf hash recursion"));
			ip_transport_redispatch(port, m, off);
		}
		return (IPPROTO_DONE);
	}

	ip = mtod(m, struct ip *);
	*mp = NULL;

	bzero(&s, sizeof s);
	s.sin_family = AF_INET;
	s.sin_len = sizeof(struct sockaddr_in);
	s.sin_addr = ip->ip_src;
	bzero(&d, sizeof d);
	d.sin_family = AF_INET;
	d.sin_len = sizeof(struct sockaddr_in);
	d.sin_addr = ip->ip_dst;

	match = NULL;
	matchprio = 0;
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != AF_INET)
			continue;
		if (ep->proto >= 0 && ep->proto != proto)
			continue;
		if (ep->func)
			prio = (*ep->func)(m, off, proto, ep->arg);
		else {
			/*
			 * it's inbound traffic, we need to match in reverse
			 * order
			 */
			prio = mask_match(ep,
					  (struct sockaddr *)&d,
					  (struct sockaddr *)&s);
		}

		/*
		 * We prioritize the matches by using bit length of the
		 * matches.  mask_match() and user-supplied matching function
		 * should return the bit length of the matches (for example,
		 * if both src/dst are matched for IPv4, 64 should be returned).
		 * 0 or negative return value means "it did not match".
		 *
		 * The question is, since we have two "mask" portion, we
		 * cannot really define total order between entries.
		 * For example, which of these should be preferred?
		 * mask_match() returns 48 (32 + 16) for both of them.
		 *	src=3ffe::/16, dst=3ffe:501::/32
		 *	src=3ffe:501::/32, dst=3ffe::/16
		 *
		 * We need to loop through all the possible candidates
		 * to get the best match - the search takes O(n) for
		 * n attachments (i.e. interfaces).
		 */
		if (prio <= 0)
			continue;
		if (prio > matchprio) {
			matchprio = prio;
			match = ep;
		}
	}

	if (match) {
		/* found a match, "match" has the best one */
		psw = match->psw;
		if (psw && psw->pr_input) {
			encap_fillarg(m, match);
			*mp = m;
			(*psw->pr_input)(mp, offp, proto);
		} else {
			m_freem(m);
		}
		return(IPPROTO_DONE);
	}

	/* for backward compatibility */
	if (proto == IPPROTO_IPV4 && ipip_input) {
		*mp = m;
		ipip_input(mp, offp, proto);
		return(IPPROTO_DONE);
	}

	/* last resort: inject to raw socket */
	*mp = m;
	rip_input(mp, offp, proto);
	return(IPPROTO_DONE);
}
#endif

#ifdef INET6
int
encap6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;
	struct sockaddr_in6 s, d;
	const struct protosw *psw;
	struct encaptab *ep, *match;
	int prio, matchprio;

	ip6 = mtod(m, struct ip6_hdr *);

	bzero(&s, sizeof s);
	s.sin6_family = AF_INET6;
	s.sin6_len = sizeof(struct sockaddr_in6);
	s.sin6_addr = ip6->ip6_src;
	bzero(&d, sizeof d);
	d.sin6_family = AF_INET6;
	d.sin6_len = sizeof(struct sockaddr_in6);
	d.sin6_addr = ip6->ip6_dst;

	match = NULL;
	matchprio = 0;
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != AF_INET6)
			continue;
		if (ep->proto >= 0 && ep->proto != proto)
			continue;
		if (ep->func)
			prio = (*ep->func)(m, *offp, proto, ep->arg);
		else {
			/*
			 * it's inbound traffic, we need to match in reverse
			 * order
			 */
			prio = mask_match(ep, (struct sockaddr *)&d,
			    (struct sockaddr *)&s);
		}

		/* see encap4_input() for issues here */
		if (prio <= 0)
			continue;
		if (prio > matchprio) {
			matchprio = prio;
			match = ep;
		}
	}

	if (match) {
		/* found a match */
		psw = match->psw;
		if (psw && psw->pr_input) {
			encap_fillarg(m, match);
			return (*psw->pr_input)(mp, offp, proto);
		} else {
			m_freem(m);
			return IPPROTO_DONE;
		}
	}

	/* last resort: inject to raw socket */
	return rip6_input(mp, offp, proto);
}
#endif

static void
encap_add(struct encaptab *ep)
{

	LIST_INSERT_HEAD(&encaptab, ep, chain);
}

/*
 * sp (src ptr) is always my side, and dp (dst ptr) is always remote side.
 * length of mask (sm and dm) is assumed to be same as sp/dp.
 * Return value will be necessary as input (cookie) for encap_detach().
 */
const struct encaptab *
encap_attach(int af, int proto, const struct sockaddr *sp,
	     const struct sockaddr *sm, const struct sockaddr *dp,
	     const struct sockaddr *dm, const struct protosw *psw, void *arg)
{
	struct encaptab *ep;

	crit_enter();
	/* sanity check on args */
	if (sp->sa_len > sizeof ep->src || dp->sa_len > sizeof ep->dst)
		goto fail;
	if (sp->sa_len != dp->sa_len)
		goto fail;
	if (af != sp->sa_family || af != dp->sa_family)
		goto fail;

	/* check if anyone have already attached with exactly same config */
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != af)
			continue;
		if (ep->proto != proto)
			continue;
		if (ep->src.ss_len != sp->sa_len ||
		    bcmp(&ep->src, sp, sp->sa_len) != 0 ||
		    bcmp(&ep->srcmask, sm, sp->sa_len) != 0)
			continue;
		if (ep->dst.ss_len != dp->sa_len ||
		    bcmp(&ep->dst, dp, dp->sa_len) != 0 ||
		    bcmp(&ep->dstmask, dm, dp->sa_len) != 0)
			continue;

		goto fail;
	}

	ep = kmalloc(sizeof *ep, M_IPENCAP, M_INTWAIT | M_ZERO | M_NULLOK);
	if (ep == NULL)
		goto fail;

	ep->af = af;
	ep->proto = proto;
	bcopy(sp, &ep->src, sp->sa_len);
	bcopy(sm, &ep->srcmask, sp->sa_len);
	bcopy(dp, &ep->dst, dp->sa_len);
	bcopy(dm, &ep->dstmask, dp->sa_len);
	ep->psw = psw;
	ep->arg = arg;

	encap_add(ep);

	crit_exit();
	return ep;

fail:
	crit_exit();
	return NULL;
}

const struct encaptab *
encap_attach_func(int af, int proto,
		  int (*func)(const struct mbuf *, int, int, void *),
		  const struct protosw *psw, void *arg)
{
	struct encaptab *ep;

	crit_enter();
	/* sanity check on args */
	if (!func)
		goto fail;

	ep = kmalloc(sizeof *ep, M_IPENCAP, M_INTWAIT | M_ZERO | M_NULLOK);
	if (ep == NULL)
		goto fail;

	ep->af = af;
	ep->proto = proto;
	ep->func = func;
	ep->psw = psw;
	ep->arg = arg;

	encap_add(ep);

	crit_exit();
	return ep;

fail:
	crit_exit();
	return NULL;
}

int
encap_detach(const struct encaptab *cookie)
{
	const struct encaptab *ep = cookie;
	struct encaptab *p;

	for (p = LIST_FIRST(&encaptab); p; p = LIST_NEXT(p, chain)) {
		if (p == ep) {
			LIST_REMOVE(p, chain);
			kfree(p, M_IPENCAP);	/*XXX*/
			return 0;
		}
	}

	return EINVAL;
}

static int
mask_match(const struct encaptab *ep, const struct sockaddr *sp,
	   const struct sockaddr *dp)
{
	struct sockaddr_storage s;
	struct sockaddr_storage d;
	int i;
	const u_int8_t *p, *q;
	u_int8_t *r;
	int matchlen;

	if (sp->sa_len > sizeof s || dp->sa_len > sizeof d)
		return 0;
	if (sp->sa_family != ep->af || dp->sa_family != ep->af)
		return 0;
	if (sp->sa_len != ep->src.ss_len || dp->sa_len != ep->dst.ss_len)
		return 0;

	matchlen = 0;

	p = (const u_int8_t *)sp;
	q = (const u_int8_t *)&ep->srcmask;
	r = (u_int8_t *)&s;
	for (i = 0 ; i < sp->sa_len; i++) {
		r[i] = p[i] & q[i];
		/* XXX estimate */
		matchlen += (q[i] ? 8 : 0);
	}

	p = (const u_int8_t *)dp;
	q = (const u_int8_t *)&ep->dstmask;
	r = (u_int8_t *)&d;
	for (i = 0 ; i < dp->sa_len; i++) {
		r[i] = p[i] & q[i];
		/* XXX rough estimate */
		matchlen += (q[i] ? 8 : 0);
	}

	/* need to overwrite len/family portion as we don't compare them */
	s.ss_len = sp->sa_len;
	s.ss_family = sp->sa_family;
	d.ss_len = dp->sa_len;
	d.ss_family = dp->sa_family;

	if (bcmp(&s, &ep->src, ep->src.ss_len) == 0 &&
	    bcmp(&d, &ep->dst, ep->dst.ss_len) == 0) {
		return matchlen;
	} else
		return 0;
}

static void
encap_fillarg(struct mbuf *m, const struct encaptab *ep)
{
	struct m_tag *tag;

	tag = m_tag_get(PACKET_TAG_ENCAP, sizeof(void *), M_NOWAIT);
	if (tag != NULL) {
		*(void **)m_tag_data(tag) = ep->arg;
		m_tag_prepend(m, tag);
	}
}

void *
encap_getarg(struct mbuf *m)
{
	void *p = NULL;
	struct m_tag *tag;

	tag = m_tag_find(m, PACKET_TAG_ENCAP, NULL);
	if (tag != NULL) {
		p = *(void **)m_tag_data(tag);
		m_tag_delete(m, tag);
	}
	return p;
}
