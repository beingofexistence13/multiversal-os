KMOD	 = wlan_tkip
SRCS	 = ieee80211_crypto_tkip.c
SRCS	+= opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_SUPPORT_TDMA    1" > ${.TARGET}
	echo "#define IEEE80211_SUPPORT_MESH    1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
