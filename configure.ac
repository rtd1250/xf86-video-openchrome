#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script

# Initialize Autoconf
AC_PREREQ(2.57)
AC_INIT([xf86-video-openchrome],
        [0.3.0],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg&component=Driver/openchrome],
        [xf86-video-openchrome])

AC_DEFINE_UNQUOTED([VIA_MAJOR_VERSION],
                   [$(echo $PACKAGE_VERSION | sed -e 's/^\([[0-9]]*\)\.[[0-9]]*\.[[0-9]]*/\1/')],
                   [Major version])
AC_DEFINE_UNQUOTED([VIA_MINOR_VERSION],
                   [$(echo $PACKAGE_VERSION | sed -e 's/^[[0-9]]*\.\([[0-9]]*\)\.[[0-9]]*/\1/')],
                   [Minor version])
AC_DEFINE_UNQUOTED([VIA_PATCHLEVEL],
                   [$(echo $PACKAGE_VERSION | sed -e 's/^[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)/\1/')],
                   [Patch version])

AC_CONFIG_SRCDIR([Makefile.am])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR(.)

AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])

XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SYS_LARGEFILE

AH_TOP([#include "xorg-server.h"])

AC_ARG_WITH(xorg-module-dir,
            AC_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
				    [Enable debugging support [[default=no]]]),
              [DEBUG="$enableval"],
              [DEBUG=no])

AC_ARG_ENABLE(xv-debug, AC_HELP_STRING([--enable-xv-debug],
				    [Enable XVideo debugging support [[default=no]]]),
              [XV_DEBUG="$enableval"],
              [XV_DEBUG=no])

AC_ARG_ENABLE(viaregtool, AC_HELP_STRING([--enable-viaregtool],
				    [Enable build of registers dumper tool [[default=no]]]),
              [TOOLS="$enableval"],
              [TOOLS=no])

# Checks for extensions
XORG_DRIVER_CHECK_EXT(RANDR, randrproto)
XORG_DRIVER_CHECK_EXT(RENDER, renderproto)
XORG_DRIVER_CHECK_EXT(XV, videoproto)
XORG_DRIVER_CHECK_EXT(XF86DRI, xextproto x11)
XORG_DRIVER_CHECK_EXT(DPMSExtension, xextproto)

# Checks for pkg-config packages
PKG_CHECK_MODULES(XORG, [xorg-server xproto fontsproto libdrm $REQUIRED_MODULES])
PKG_CHECK_MODULES(XEXT, [xextproto >= 7.0.99.1],
 HAVE_XEXTPROTO_71="yes"; AC_DEFINE(HAVE_XEXTPROTO_71, 1, [xextproto 7.1 available]),
 HAVE_XEXTPROTO_71="no")
AM_CONDITIONAL(HAVE_XEXTPROTO_71, [ test "$HAVE_XEXTPROTO_71" = "yes" ])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$XORG_CFLAGS $CPPFLAGS"
AC_CHECK_HEADER(xf86Modes.h,[XMODES=yes],[XMODES=no],[#include "xorg-server.h"])
AC_CHECK_DECL(XSERVER_LIBPCIACCESS,
             [XSERVER_LIBPCIACCESS=yes],[XSERVER_LIBPCIACCESS=no],
             [#include "xorg-server.h"])
CPPFLAGS="$save_CPPFLAGS"

if test x$XSERVER_LIBPCIACCESS = xyes; then
       PKG_CHECK_MODULES([PCIACCESS], [pciaccess >= 0.8.0])
fi

PKG_CHECK_MODULES(DRI, [libdrm >= 2.0 xf86driproto])
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$XORG_CFLAGS $DRI_CFLAGS"
AC_CHECK_HEADER([dri.h],
              [have_dri_h="yes"], [have_dri_h="no"],[-])
AC_CHECK_HEADER([sarea.h],
              [have_sarea_h="yes"], [have_sarea_h="no"],[-])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[
#include <xorg-server.h>
#include <dristruct.h>
              ]])],
              [have_dristruct_h="yes"], [have_dristruct_h="no"],[-])
AC_CHECK_HEADER([damage.h],
              [have_damage_h="yes"], [have_damage_h="no"],[-])
CPPFLAGS="$save_CPPFLAGS"

AC_MSG_CHECKING([whether to include DRI support])
if test "$have_dri_h" = yes -a \
        "$have_sarea_h" = yes -a \
        "$have_dristruct_h" = yes; then
        DRI="yes"
else
        DRI="no"
fi
AC_MSG_RESULT([$DRI])

AM_CONDITIONAL(DRI, test x$DRI = xyes)
if test "$DRI" = yes; then
        AC_DEFINE(XF86DRI,1,[Enable DRI driver support])
        AC_DEFINE(XF86DRI_DEVEL,1,[Enable developmental DRI driver support])
        if test "$have_damage_h" = yes; then
                AC_DEFINE(DAMAGE,1,[Use Damage extension])
        fi

        save_CFLAGS="$CFLAGS"
        CFLAGS="$XORG_CFLAGS $DRI_CFLAGS $CFLAGS"
AC_CHECK_HEADER(xf86drmMode.h,[DRM_MODE=yes],[DRM_MODE=no],[#include <stdint.h>
#include <stdlib.h>])
        if test "x$DRM_MODE" = xyes; then
            PKG_CHECK_MODULES(LIBUDEV, [libudev], [LIBUDEV=yes], [LIBUDEV=no])
            if test "x$LIBUDEV" = xyes; then
                AC_DEFINE(HAVE_LIBUDEV, 1,[libudev support])
            fi
        fi
        CFLAGS="$save_CFLAGS"

        case "$host_cpu" in
            i*86)
                XVMC=yes ;;
            amd64*|x86_64*)
                XVMC=yes ;;
            *)
                XVMC=no ;;
        esac
else
        XVMC="no"
fi

if test "x$XVMC" = xyes; then
	AC_CHECK_HEADERS(pthread.h sys/ioctl.h sys/time.h time.h,,[XVMC="no"; break],)
	PKG_CHECK_MODULES(XVMC, [x11 xext xvmc])
fi

AC_MSG_CHECKING([whether to build XvMC driver support])
AC_MSG_RESULT([$XVMC])

AM_CONDITIONAL(XVMC, test x$XVMC = xyes)

# Check the ABI_VIDEODRV_VERSION
SAVE_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $XORG_CFLAGS"
		
AM_CONDITIONAL(XSERVER_LIBPCIACCESS, test x$XSERVER_LIBPCIACCESS = xyes)
if test "$XSERVER_LIBPCIACCESS" = yes; then
        AC_DEFINE(XSERVER_LIBPCIACCESS,1,[Enable libpciaccess])
fi

AM_CONDITIONAL(DEBUG, test x$DEBUG = xyes)
if test "$DEBUG" = yes; then
        AC_DEFINE(HAVE_DEBUG,1,[Enable debug support])
fi

AM_CONDITIONAL(XV_DEBUG, test x$XV_DEBUG = xyes)
if test "$XV_DEBUG" = yes; then
        AC_DEFINE(XV_DEBUG,1,[Enable XVideo debug support])
fi

AM_CONDITIONAL(TOOLS, test x$TOOLS = xyes)
if test "$TOOLS" = yes; then
        AC_DEFINE(TOOLS,1,[Enable build of registers dumper tool])
fi

AC_SUBST([DRI_CFLAGS])
AC_SUBST([XORG_CFLAGS])
AC_SUBST([moduledir])

DRIVER_NAME=openchrome
AC_SUBST([DRIVER_NAME])

AC_DEFINE(X_USE_LINEARFB,1,[Compatibility define for older Xen])
AC_DEFINE(X_USE_REGION_NULL,1,[Compatibility define for older Xen])
AC_DEFINE(X_HAVE_XAAGETROP,1,[Compatibility define for older Xen])
AC_DEFINE(X_NEED_I2CSTART,1,[Compatibility define for older Xen])

DRIVER_MAN_SUFFIX="4"
AC_SUBST([DRIVER_MAN_SUFFIX])

XORG_RELEASE_VERSION

AC_OUTPUT([
	Makefile
	src/xvmc/Makefile
	src/Makefile
	man/Makefile
	tools/Makefile
])
