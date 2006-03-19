if XVMC
lib_LTLIBRARIES=libviaXvMC.la libviaXvMCPro.la

libviaXvMC_la_SOURCES = \
	viaLowLevel.c \
	driDrawable.c \
	viaXvMC.c \
	xf86dri.c \
	viaLowLevel.h \
	driDrawable.h \
	viaXvMCPriv.h \
	xf86dri.h \
	xf86dristr.h \
	vldXvMC.h
libviaXvMCPro_la_SOURCES = \
	viaLowLevelPro.c \
	driDrawable.c \
	viaXvMC.c \
	xf86dri.c \
	viaLowLevel.h \
	driDrawable.h \
	viaXvMCPriv.h \
	xf86dri.h \
	xf86dristr.h \
	vldXvMC.h

libviaXvMC_la_CFLAGS = @XORG_CFLAGS@ @DRI_CFLAGS@ -I$(top_srcdir)/unichrome -I$(top_srcdir)/libxvmc -DTRUE=1 -DFALSE=0
libviaXvMC_la_LDFLAGS = @DRI_LIBS@ -version-number 1:0:0

libviaXvMCPro_la_CFLAGS = @XORG_CFLAGS@ @DRI_CFLAGS@ -I$(top_srcdir)/unichrome -I$(top_srcdir)/libxvmc -DTRUE=1 -DFALSE=0
libviaXvMCPro_la_LDFLAGS = @DRI_LIBS@ -version-number 1:0:0
else
EXTRA_DIST = \
	driDrawable.c \
	driDrawable.h \
	viaLowLevelPro.c \
	viaLowLevel.c \
	viaLowLevel.h \
	viaXvMC.c \
	viaXvMCPriv.h \
	xf86dri.c \
	xf86dri.h \
	xf86dristr.h
endif
