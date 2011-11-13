/*
 * Copyright 2011 James Simmons, All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _VIA_MEMMGR_H_
#define _VIA_MEMMGR_H_
#include "xf86.h"

struct buffer_object {
    unsigned long handle;
    unsigned long offset;
    unsigned long pitch;
    unsigned long size;
    void *ptr;
};

typedef struct {
    unsigned long   base;               /* Offset into fb */
    int     pool;                       /* Pool we drew from */
    int     size;
#ifdef XF86DRI
    int    drm_fd;                      /* Fd in DRM mode */
    drm_via_mem_t drm;                  /* DRM management object */
#endif
    void  *pVia;                        /* VIA driver pointer */
    FBLinearPtr linear;                 /* X linear pool info ptr */
    ExaOffscreenArea *exa;
    ScrnInfoPtr pScrn;
} VIAMem;

typedef VIAMem *VIAMemPtr;

/* In via_memory.c */
void VIAFreeLinear(VIAMemPtr);
int VIAAllocLinear(VIAMemPtr, ScrnInfoPtr, unsigned long);
int viaOffscreenLinear(VIAMemPtr, ScrnInfoPtr, unsigned long);
void VIAInitLinear(ScreenPtr pScreen);

#endif
