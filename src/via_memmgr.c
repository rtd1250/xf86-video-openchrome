/*
 * Copyright 2003 Red Hat, Inc. All Rights Reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86fbman.h"

#ifdef XF86DRI
#include "xf86drm.h"
#endif

#include "via_driver.h"
#include "via_swov.h"
#ifdef XF86DRI
#include "via_drm.h"
#endif

/*
 *	Isolate the wonders of X memory allocation and DRI memory allocation
 *	and 4.3 or 4.4 differences in one abstraction.
 *
 *	The pool code indicates who provided the memory:
 *	0  -  nobody
 *	1  -  xf86 linear
 *	2  -  DRM
 */

static void
viaExaFBSave(ScreenPtr pScreen, ExaOffscreenArea * exa)
{
    FatalError("Xserver is incompatible with openchrome EXA.\n"
               "\t\tPlease look at Xorg bugzilla bug #7639, and at\n"
               "\t\thttp://wiki.openchrome.org/tikiwiki/tiki-index"
               ".php?page=EXAAcceleration .\n");
}

int
viaOffScreenLinear(struct buffer_object *obj, ScrnInfoPtr pScrn, unsigned long size)
{
    int depth = pScrn->bitsPerPixel >> 3;
    VIAPtr pVia = VIAPTR(pScrn);
    FBLinearPtr linear;

    if (pVia->exaDriverPtr && !pVia->NoAccel) {

        obj->exa = exaOffscreenAlloc(pScrn->pScreen, size,
                                     32, TRUE, NULL, NULL);
        if (obj->exa == NULL)
            return BadAlloc;
        obj->exa->save = viaExaFBSave;
        obj->offset = obj->exa->offset;
        obj->size = size;
        obj->pool = 1;
        return Success;
    }

    linear = xf86AllocateOffscreenLinear(pScrn->pScreen,
                                        (size + depth - 1) / depth,
                                        32, NULL, NULL, NULL);
    if (!linear)
        return BadAlloc;
    obj->offset = linear->offset * depth;
    obj->handle = (unsigned long) linear;
    obj->size = size;
    obj->pool = 1;
    return Success;
}

struct buffer_object *
drm_bo_alloc(ScrnInfoPtr pScrn, unsigned int size)
{
    struct buffer_object *obj = NULL;
    VIAPtr pVia = VIAPTR(pScrn);
    int ret;

    obj = xnfcalloc(1, sizeof(*obj));
    if (obj) {
#ifdef XF86DRI
        if (pVia->directRenderingType == DRI_1) {
            obj->drm.context = DRIGetContext(pScrn->pScreen);
            obj->drm.size = size;
            obj->drm.type = VIA_MEM_VIDEO;
            ret = drmCommandWriteRead(pVia->drmFD, DRM_VIA_ALLOCMEM,
                                      &obj->drm, sizeof(drm_via_mem_t));
            if (ret || (size != obj->drm.size)) {
                /* Try X Offsceen fallback before failing. */
                if (Success != viaOffScreenLinear(obj, pScrn, size)) {
                    ErrorF("DRM memory allocation failed\n");
                    free(obj);
                }
                return obj;
            }
            obj->offset = obj->drm.offset;
            obj->pool = 2;
            DEBUG(ErrorF("Fulfilled via DRI at %lu\n", obj->offset));
            return obj;
        }
#endif
        if (Success != viaOffScreenLinear(obj, pScrn, size)) {
            ErrorF("Linear memory allocation failed\n");
            free(obj);
        }
    }
    return obj;
}

void*
drm_bo_map(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
    VIAPtr pVia = VIAPTR(pScrn);
    void *virtual = NULL;

    switch (obj->pool) {
    default:
        virtual = pVia->FBBase + obj->offset;
        break;
    }
    return virtual;
}

void
drm_bo_unmap(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
}

void
drm_bo_free(ScrnInfoPtr pScrn, struct buffer_object *obj)
{
    VIAPtr pVia = VIAPTR(pScrn);
    FBLinearPtr linear;

    if (obj) {
        DEBUG(ErrorF("Freed %lu (pool %d)\n", obj->offset, obj->pool));
        switch (obj->pool) {
        case 0:
            break;
        case 1:
            if (pVia->useEXA && !pVia->NoAccel) {
                exaOffscreenFree(pScrn->pScreen, obj->exa);
                obj->handle = 0;
                obj->pool = 0;
                return;
            }

            linear = (FBLinearPtr) obj->handle;
            xf86FreeOffscreenLinear(linear);
            obj->handle = 0;
            obj->pool = 0;
            break;
        case 2:
#ifdef XF86DRI
            if (pVia->directRenderingType == DRI_1) {
                if (drmCommandWrite(pVia->drmFD, DRM_VIA_FREEMEM,
                                    &obj->drm, sizeof(drm_via_mem_t)) < 0)
                    ErrorF("DRM module failed free.\n");
            }
#endif
            obj->pool = 0;
            break;
        }
    }
}

Bool
drm_bo_manager_init(ScrnInfoPtr pScrn)
{
    return ums_create(pScrn);
}
