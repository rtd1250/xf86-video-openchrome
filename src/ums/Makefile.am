#  Copyright 2011 James Simmons
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

AM_CFLAGS = @XORG_CFLAGS@ @DRI_CFLAGS@ -I$(top_srcdir)/src

noinst_LTLIBRARIES = libums.la
libums_la_LIBADD = @DRM_LIBS@

NULL:=#

libums_la_SOURCES = \
	via_id.c \
	via_vgahw.c \
	via_crtc.c \
	via_display.c \
	via_lvds.c \
	via_panel.c \
	via_ums.c \
	via_i2c.c \
	via_mode.c \
	via_mode.h \
	via_bandwidth.c \
	via_timing.c \
	via_timing.h \
	via_vbe.c \
	via_ch7xxx.c \
	via_ch7xxx.h \
	via_vt162x.c \
	via_vt162x.h \
	$(NULL)
