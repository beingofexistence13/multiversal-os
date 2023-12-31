# $FreeBSD: src/sys/modules/acpi/acpi/Makefile,v 1.3 2004/01/08 16:38:32 njl Exp $

SYSDIR?= ${.CURDIR}/../..

.if ACPI_DEBUG_LOCKS
KCFLAGS+=	-DACPI_DEBUG_LOCKS
.endif
.if ACPI_DEBUG_MEMMAP
KCFLAGS+=	-DACPI_DEBUG_MEMMAP
.endif
.if ACPI_DEBUG_CACHE
KCFLAGS+=	-DACPI_DEBUG_CACHE
.endif

.include "Makefile.inc"

.PATH:	${.OBJDIR}					\
	${SYSDIR}/${OSACPI_MD_DIR}			\
	${SYSDIR}/platform/${MACHINE_PLATFORM}/isa	\
	${.CURDIR}/Osd					\
	${ACPICA_KERN_PATHS}

KMOD	= acpi

# ACPICA sources
SRCS+=	ahids.c ahuuids.c
SRCS+=	dbcmds.c dbconvert.c dbdisply.c dbexec.c dbfileio.c dbhistry.c
SRCS+=	dbinput.c dbmethod.c dbnames.c dbobject.c
SRCS+=	dbstats.c dbtest.c dbutils.c dbxface.c
SRCS+=	dmbuffer.c dmcstyle.c dmdeferred.c dmnames.c dmopcode.c
SRCS+=	dmresrc.c dmresrcl.c dmresrcl2.c dmresrcs.c dmutils.c dmwalk.c
SRCS+=	dsdebug.c dsfield.c dsinit.c dsmethod.c dsmthdat.c
SRCS+=	dsobject.c dsopcode.c dspkginit.c dsutils.c dswexec.c dswload.c
SRCS+=	dswscope.c dswstate.c dsargs.c dscontrol.c dswload2.c
SRCS+=	evevent.c evglock.c evgpe.c evgpeblk.c evgpeinit.c evgpeutil.c
SRCS+=	evhandler.c evmisc.c evregion.c evrgnini.c evsci.c evxface.c evxfgpe.c
SRCS+=	evxfevnt.c evxfregn.c
SRCS+=	exconcat.c exconfig.c exconvrt.c excreate.c
SRCS+=	exdump.c exfield.c exfldio.c exmisc.c exmutex.c
SRCS+=	exnames.c exoparg1.c exoparg2.c exoparg3.c exoparg6.c
SRCS+=	exprep.c exregion.c exresnte.c exresolv.c exresop.c
SRCS+=	exserial.c exstore.c exstoren.c exstorob.c exsystem.c
SRCS+=	extrace.c exutils.c exdebug.c
SRCS+=	hwacpi.c hwesleep.c hwgpe.c hwregs.c hwsleep.c
SRCS+=	hwtimer.c hwvalid.c hwxface.c hwpci.c hwxfsleep.c
SRCS+=	nsarguments.c nsaccess.c nsalloc.c nsconvert.c nsdump.c nseval.c
SRCS+=	nsinit.c nsload.c nsnames.c nsobject.c nsparse.c nspredef.c nsprepkg.c
SRCS+=	nsrepair.c nsrepair2.c nssearch.c
SRCS+=	nsutils.c nswalk.c nsxfeval.c nsxfname.c nsxfobj.c
SRCS+=	psargs.c psloop.c psobject.c psopcode.c psopinfo.c psparse.c psscope.c
SRCS+=	pstree.c psutils.c pswalk.c psxface.c
SRCS+=	rsaddr.c rscalc.c rscreate.c rsdump.c rsdumpinfo.c rsinfo.c rsio.c
SRCS+=	rsirq.c rslist.c rsmemory.c rsmisc.c rsserial.c rsutils.c
SRCS+=	rsxface.c tbdata.c tbfadt.c tbfind.c tbinstal.c tbprint.c
SRCS+=	tbutils.c tbxface.c tbxfload.c tbxfroot.c
SRCS+=	utalloc.c utascii.c utbuffer.c utaddress.c utcopy.c
SRCS+=	utdebug.c utdecode.c utdelete.c uterror.c uteval.c utexcep.c
SRCS+=	utglobal.c uthex.c utids.c utinit.c utlock.c
SRCS+=	utmath.c utmisc.c utmutex.c utnonansi.c
SRCS+=	utobject.c utosi.c utownerid.c utpredef.c
SRCS+=	utresrc.c utresdecode.c utstate.c utstring.c utstrsuppt.c utstrtoul64.c
SRCS+=	utuuid.c utxface.c utxferror.c utxfinit.c

# OSD layer
#
SRCS+=  acpi.c acpi_acad.c acpi_battery.c acpi_button.c acpi_cmbat.c
SRCS+=	acpi_cpu.c acpi_cpu_cstate.c acpi_cpu_pstate.c acpi_smbat.c
SRCS+=  acpi_ec.c acpi_isab.c acpi_lid.c
SRCS+=  acpi_package.c
SRCS+=	acpi_pci.c acpi_pcib.c acpi_pcib_acpi.c acpi_pcib_pci.c
SRCS+=	acpi_pci_link.c
SRCS+=  acpi_powerres.c acpi_quirk.c acpi_resource.c acpi_thermal.c
SRCS+=  acpi_timer.c acpi_hpet.c acpi_if.c acpi_wmi_if.c
SRCS+=  OsdCache.c OsdDebug.c
SRCS+=  OsdHardware.c OsdInterrupt.c OsdMemory.c OsdSchedule.c
SRCS+=  OsdStream.c OsdSynch.c OsdTable.c OsdEnvironment.c
SRCS+=  opt_acpi.h opt_ddb.h opt_ktr.h
SRCS+=  device_if.h bus_if.h pci_if.h pcib_if.h isa_if.h acpi_if.h
SRCS+=  acpi_quirks.h acpi_wmi_if.h cpu_if.h

.if !defined(KERNBUILDDIR)
.if ACPI_DEBUG
KCFLAGS+=-DACPI_DEBUG
opt_ddb.h: Makefile
	echo "#define DDB 1" > ${.TARGET}
.else
opt_ddb.h: Makefile
	echo -n > ${.TARGET}
.endif
.endif

# Machine-specific code for P-State
SRCS+=	acpi_pstate_machdep.c
# Machine-specific code for C-State
SRCS+=	acpi_cstate_machdep.c
# Machine-specific code for CPU
SRCS+=	acpi_cpu_machdep.c
# Machine-specific code such as sleep/wakeup
SRCS+=	acpi_machdep.c acpi_wakeup.c
.if ${MACHINE_ARCH} == "x86_64"
SRCS+=	pmtimer.c
.endif
#SRCS+=	acpi_wakecode.h
#CLEANFILES+=	acpi_wakecode.h acpi_wakecode.o acpi_wakecode.bin

#acpi_wakecode.h: acpi_wakecode.bin
#	file2c -sx 'static char wakecode[] = {' '};' < acpi_wakecode.bin > ${.TARGET}
#acpi_wakecode.bin: acpi_wakecode.o
#	objcopy -S -O binary acpi_wakecode.o ${.TARGET}

#acpi_wakecode.o: acpi_wakecode.S

SUBDIR=	acpi_asus acpi_dock acpi_fujitsu acpi_hp acpi_panasonic acpi_pvpanic
SUBDIR+=acpi_sony acpi_thinkpad acpi_toshiba acpi_video acpi_wmi aibs

# Empty for concurrent build
#
SUBDIR_ORDERED=

.include <bsd.kmod.mk>
