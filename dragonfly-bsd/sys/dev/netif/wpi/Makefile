KMOD	= if_wpi
SRCS	= if_wpi.c device_if.h bus_if.h pci_if.h opt_wlan.h opt_wpi.h

SUBDIR	= fw

.ifndef KERNBUILDDIR

opt_wpi.h:
	touch ${.OBJDIR}/${.TARGET}

.endif

.include <bsd.kmod.mk>
