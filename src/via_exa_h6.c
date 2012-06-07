/*
 * Copyright 1998-2008 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 * Copyright 2006 Thomas Hellström. All Rights Reserved.
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

/*
 * 2D acceleration functions for the VIA/S3G UniChrome IGPs.
 *
 * Mostly rewritten, and modified for EXA support, by Thomas Hellström.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xarch.h>
#include "miline.h"

#include "via_driver.h"
#include "via_regs.h"
#include "via_dmabuffer.h"
#include "via_rop.h"

/*
 * This is a small helper to wrap around a PITCH register write
 * to deal with the subtle differences of M1 and old 2D engine
 */
static void
viaPitchHelper_H6(VIAPtr pVia, unsigned dstPitch, unsigned srcPitch)
{
    unsigned val = (dstPitch >> 3) << 16 | (srcPitch >> 3);
    RING_VARS;

    if (pVia->Chipset != VIA_VX800 &&
        pVia->Chipset != VIA_VX855 &&
        pVia->Chipset != VIA_VX900) {
        val |= VIA_PITCH_ENABLE;
    }
    OUT_RING_H1(VIA_REG_PITCH_M1, val);
}

/*
 * Emit clipping borders to the command buffer and update the 2D context
 * current command with clipping info.
 */
static int
viaAccelClippingHelper_H6(VIAPtr pVia, int refY)
{
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    if (tdc->clipping) {
        refY = (refY < tdc->clipY1) ? refY : tdc->clipY1;
        tdc->cmd |= VIA_GEC_CLIP_ENABLE;
        BEGIN_RING(4);
        OUT_RING_H1(VIA_REG_CLIPTL_M1,
                    ((tdc->clipY1 - refY) << 16) | tdc->clipX1);
        OUT_RING_H1(VIA_REG_CLIPBR_M1,
		    ((tdc->clipY2 - refY) << 16) | tdc->clipX2);
    } else {
        tdc->cmd &= ~VIA_GEC_CLIP_ENABLE;
    }
    return refY;
}

/*
 * Emit a solid blit operation to the command buffer.
 */
void
viaAccelSolidHelper_H6(VIAPtr pVia, int x, int y, int w, int h,
                        unsigned fbBase, CARD32 mode, unsigned pitch,
                        CARD32 fg, CARD32 cmd)
{
    RING_VARS;

    BEGIN_RING(14);
    OUT_RING_H1(VIA_REG_GEMODE_M1, mode);
    OUT_RING_H1(VIA_REG_DSTBASE_M1, fbBase >> 3);
    viaPitchHelper_H6(pVia, pitch, 0);
    OUT_RING_H1(VIA_REG_DSTPOS_M1, (y << 16) | (x & 0xFFFF));
    OUT_RING_H1(VIA_REG_DIMENSION_M1, ((h - 1) << 16) | (w - 1));
    OUT_RING_H1(VIA_REG_MONOPATFGC_M1, fg);
    OUT_RING_H1(VIA_REG_GECMD_M1, cmd);
}

/*
 * Check if we can use a planeMask and update the 2D context accordingly.
 */
static Bool
viaAccelPlaneMaskHelper_H6(ViaTwodContext * tdc, CARD32 planeMask)
{
    CARD32 modeMask = (1 << ((1 << tdc->bytesPPShift) << 3)) - 1;
    CARD32 curMask = 0x00000000;
    CARD32 curByteMask;
    int i;

    if ((planeMask & modeMask) != modeMask) {

        /* Masking doesn't work in 8bpp. */
        if (modeMask == 0xFF) {
            tdc->keyControl &= 0x0FFFFFFF;
            return FALSE;
        }

        /* Translate the bit planemask to a byte planemask. */
        for (i = 0; i < (1 << tdc->bytesPPShift); ++i) {
            curByteMask = (0xFF << (i << 3));

            if ((planeMask & curByteMask) == 0) {
                curMask |= (1 << i);
            } else if ((planeMask & curByteMask) != curByteMask) {
                tdc->keyControl &= 0x0FFFFFFF;
                return FALSE;
            }
        }
        ErrorF("DEBUG: planeMask 0x%08x, curMask 0%02x\n",
               (unsigned)planeMask, (unsigned)curMask);

        tdc->keyControl = (tdc->keyControl & 0x0FFFFFFF) | (curMask << 28);
    }

    return TRUE;
}

/*
 * Emit transparency state and color to the command buffer.
 */
static void
viaAccelTransparentHelper_H6(VIAPtr pVia, CARD32 keyControl,
                          CARD32 transColor, Bool usePlaneMask)
{
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    tdc->keyControl &= ((usePlaneMask) ? 0xF0000000 : 0x00000000);
    tdc->keyControl |= (keyControl & 0x0FFFFFFF);
    BEGIN_RING(4);
    OUT_RING_H1(VIA_REG_KEYCONTROL_M1, tdc->keyControl);
    if (keyControl) {
        OUT_RING_H1(VIA_REG_SRCCOLORKEY_M1, transColor);
    }
}

/*
 * Emit a copy blit operation to the command buffer.
 */
static void
viaAccelCopyHelper_H6(VIAPtr pVia, int xs, int ys, int xd, int yd,
                   int w, int h, unsigned srcFbBase, unsigned dstFbBase,
                   CARD32 mode, unsigned srcPitch, unsigned dstPitch,
                   CARD32 cmd)
{
    RING_VARS;

    if (cmd & VIA_GEC_DECY) {
        ys += h - 1;
        yd += h - 1;
    }

    if (cmd & VIA_GEC_DECX) {
        xs += w - 1;
        xd += w - 1;
    }

    BEGIN_RING(16);
    OUT_RING_H1(VIA_REG_GEMODE_M1, mode);
    OUT_RING_H1(VIA_REG_SRCBASE_M1, srcFbBase >> 3);
    OUT_RING_H1(VIA_REG_DSTBASE_M1, dstFbBase >> 3);
    viaPitchHelper_H6(pVia, dstPitch, srcPitch);
    OUT_RING_H1(VIA_REG_SRCPOS_M1, (ys << 16) | (xs & 0xFFFF));
    OUT_RING_H1(VIA_REG_DSTPOS_M1, (yd << 16) | (xd & 0xFFFF));
    OUT_RING_H1(VIA_REG_DIMENSION_M1, ((h - 1) << 16) | (w - 1));
    OUT_RING_H1(VIA_REG_GECMD_M1, cmd);
}

/*
 * Mark Sync using the 2D blitter for AGP. NoOp for PCI.
 * In the future one could even launch a NULL PCI DMA command
 * to have an interrupt generated, provided it is possible to
 * write to the PCI DMA engines from the AGP command stream.
 */
int
viaAccelMarkSync_H6(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    RING_VARS;

    ++pVia->curMarker;

    /* Wrap around without affecting the sign bit. */
    pVia->curMarker &= 0x7FFFFFFF;

    if (pVia->agpDMA) {
        BEGIN_RING(2);
        OUT_RING_H1(VIA_REG_KEYCONTROL_M1, 0x00);
        viaAccelSolidHelper_H6(pVia, 0, 0, 1, 1, pVia->markerOffset,
                            VIA_GEM_32bpp, 4, pVia->curMarker,
                            (0xF0 << 24) | VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT);
        ADVANCE_RING;
    }
    return pVia->curMarker;
}

/*
 * Exa functions. It is assumed that EXA does not exceed the blitter limits.
 */
Bool
viaExaPrepareSolid_H6(PixmapPtr pPixmap, int alu, Pixel planeMask, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    if (exaGetPixmapPitch(pPixmap) & 7)
        return FALSE;

    if (!viaAccelSetMode(pPixmap->drawable.depth, tdc))
        return FALSE;

    if (!viaAccelPlaneMaskHelper_H6(tdc, planeMask))
        return FALSE;

    viaAccelTransparentHelper_H6(pVia, 0x0, 0x0, TRUE);

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(alu);

    tdc->fgColor = fg;

    return TRUE;
}

void
viaExaSolid_H6(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstPitch, dstOffset;

    RING_VARS;

    int w = x2 - x1, h = y2 - y1;

    dstPitch = exaGetPixmapPitch(pPixmap);
    dstOffset = exaGetPixmapOffset(pPixmap);

    viaAccelSolidHelper_H6(pVia, x1, y1, w, h, dstOffset, tdc->mode,
                            dstPitch, tdc->fgColor, tdc->cmd);
    ADVANCE_RING;
}

void
viaExaDoneSolidCopy_H6(PixmapPtr pPixmap)
{
}

Bool
viaExaPrepareCopy_H6(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir,
                        int ydir, int alu, Pixel planeMask)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    if (pSrcPixmap->drawable.bitsPerPixel != pDstPixmap->drawable.bitsPerPixel)
        return FALSE;

    if ((tdc->srcPitch = exaGetPixmapPitch(pSrcPixmap)) & 3)
        return FALSE;

    if (exaGetPixmapPitch(pDstPixmap) & 7)
        return FALSE;

    tdc->srcOffset = exaGetPixmapOffset(pSrcPixmap);

    tdc->cmd = VIA_GEC_BLT | VIAACCELCOPYROP(alu);
    if (xdir < 0)
        tdc->cmd |= VIA_GEC_DECX;
    if (ydir < 0)
        tdc->cmd |= VIA_GEC_DECY;

    if (!viaAccelSetMode(pDstPixmap->drawable.bitsPerPixel, tdc))
        return FALSE;

    if (!viaAccelPlaneMaskHelper_H6(tdc, planeMask))
        return FALSE;
    viaAccelTransparentHelper_H6(pVia, 0x0, 0x0, TRUE);

    return TRUE;
}

void
viaExaCopy_H6(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY,
                int width, int height)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    CARD32 srcOffset = tdc->srcOffset;
    CARD32 dstOffset = exaGetPixmapOffset(pDstPixmap);

    RING_VARS;

    if (!width || !height)
        return;

    viaAccelCopyHelper_H6(pVia, srcX, srcY, dstX, dstY, width, height,
                            srcOffset, dstOffset, tdc->mode, tdc->srcPitch,
                            exaGetPixmapPitch(pDstPixmap), tdc->cmd);
    ADVANCE_RING;
}

Bool
viaExaCheckComposite_H6(int op, PicturePtr pSrcPicture,
                        PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPicture->pDrawable->pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    Via3DState *v3d = &pVia->v3d;

    /* Reject small composites early. They are done much faster in software. */
    if (!pSrcPicture->repeat &&
        pSrcPicture->pDrawable->width *
        pSrcPicture->pDrawable->height < VIA_MIN_COMPOSITE)
        return FALSE;

    if (pMaskPicture &&
        !pMaskPicture->repeat &&
        pMaskPicture->pDrawable->width *
        pMaskPicture->pDrawable->height < VIA_MIN_COMPOSITE)
        return FALSE;

    if (pMaskPicture && pMaskPicture->repeat != RepeatNormal)
        return FALSE;

    if (pMaskPicture && pMaskPicture->componentAlpha) {
#ifdef VIA_DEBUG_COMPOSITE
        ErrorF("Component Alpha operation\n");
#endif
        return FALSE;
    }

    if (!v3d->opSupported(op)) {
#ifdef VIA_DEBUG_COMPOSITE
#warning Composite verbose debug turned on.
        ErrorF("Operator not supported\n");
        viaExaPrintComposite(op, pSrcPicture, pMaskPicture, pDstPicture);
#endif
        return FALSE;
    }

    /*
     * FIXME: A8 destination formats are currently not supported and do not
     * seem supported by the hardware, although there are some leftover
     * register settings apparent in the via_3d_reg.h file. We need to fix this
     * (if important), by using component ARGB8888 operations with bitmask.
     */

    if (!v3d->dstSupported(pDstPicture->format)) {
#ifdef VIA_DEBUG_COMPOSITE
        ErrorF("Destination format not supported:\n");
        viaExaPrintComposite(op, pSrcPicture, pMaskPicture, pDstPicture);
#endif
        return FALSE;
    }

    if (v3d->texSupported(pSrcPicture->format)) {
        if (pMaskPicture && (PICT_FORMAT_A(pMaskPicture->format) == 0 ||
                             !v3d->texSupported(pMaskPicture->format))) {
#ifdef VIA_DEBUG_COMPOSITE
            ErrorF("Mask format not supported:\n");
            viaExaPrintComposite(op, pSrcPicture, pMaskPicture, pDstPicture);
#endif
            return FALSE;
        }
        return TRUE;
    }
#ifdef VIA_DEBUG_COMPOSITE
    ErrorF("Src format not supported:\n");
    viaExaPrintComposite(op, pSrcPicture, pMaskPicture, pDstPicture);
#endif
    return FALSE;
}

static Bool
viaIsAGP(VIAPtr pVia, PixmapPtr pPix, unsigned long *offset)
{
#ifdef XF86DRI
    unsigned long offs;

    if (pVia->directRenderingType && !pVia->IsPCI) {
        offs = ((unsigned long)pPix->devPrivate.ptr
                - (unsigned long)pVia->agpMappedAddr);

        if ((offs - pVia->scratchOffset) < pVia->agpSize) {
            *offset = offs + pVia->agpAddr;
            return TRUE;
        }
    }
#endif
    return FALSE;
}

static Bool
viaExaIsOffscreen(PixmapPtr pPix)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    return ((unsigned long)pPix->devPrivate.ptr -
            (unsigned long) drm_bo_map(pScrn, pVia->drmmode.front_bo)) < pVia->drmmode.front_bo->size;
}

Bool
viaExaPrepareComposite_H6(int op, PicturePtr pSrcPicture,
                            PicturePtr pMaskPicture, PicturePtr pDstPicture,
                            PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    CARD32 height, width;
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    Via3DState *v3d = &pVia->v3d;
    int curTex = 0;
    ViaTexBlendingModes srcMode;
    Bool isAGP;
    unsigned long offset;

    /* Workaround: EXA crash with new libcairo2 on a VIA VX800 (#298) */
    /* TODO Add real source only pictures */
    if (!pSrc) {
	    ErrorF("pSrc is NULL\n");
	    return FALSE;
	}

    v3d->setDestination(v3d, exaGetPixmapOffset(pDst),
                        exaGetPixmapPitch(pDst), pDstPicture->format);
    v3d->setCompositeOperator(v3d, op);
    v3d->setDrawing(v3d, 0x0c, 0xFFFFFFFF, 0x000000FF, 0xFF);

    viaOrder(pSrc->drawable.width, &width);
    viaOrder(pSrc->drawable.height, &height);

    /*
     * For one-pixel repeat mask pictures we avoid using multitexturing by
     * modifying the src's texture blending equation and feed the pixel
     * value as a constant alpha for the src's texture. Multitexturing on the
     * Unichromes seems somewhat slow, so this speeds up translucent windows.
     */

    srcMode = via_src;
    pVia->maskP = NULL;
    if (pMaskPicture &&
        (pMaskPicture->pDrawable->height == 1) &&
        (pMaskPicture->pDrawable->width == 1) &&
        pMaskPicture->repeat && viaExpandablePixel(pMaskPicture->format)) {
        pVia->maskP = pMask->devPrivate.ptr;
        pVia->maskFormat = pMaskPicture->format;
        pVia->componentAlpha = pMaskPicture->componentAlpha;
        srcMode = ((pMaskPicture->componentAlpha)
                   ? via_src_onepix_comp_mask : via_src_onepix_mask);
    }

    /*
     * One-Pixel repeat src pictures go as solid color instead of textures.
     * Speeds up window shadows.
     */

    pVia->srcP = NULL;
    if (pSrcPicture && pSrcPicture->repeat
        && (pSrcPicture->pDrawable->height == 1)
        && (pSrcPicture->pDrawable->width == 1)
        && viaExpandablePixel(pSrcPicture->format)) {
        pVia->srcP = pSrc->devPrivate.ptr;
        pVia->srcFormat = pSrcPicture->format;
    }

    /* Exa should be smart enough to eliminate this IN operation. */
    if (pVia->srcP && pVia->maskP) {
        ErrorF("Bad one-pixel IN composite operation. "
               "EXA needs to be smarter.\n");
        return FALSE;
    }

    if (!pVia->srcP) {
        offset = exaGetPixmapOffset(pSrc);
        isAGP = viaIsAGP(pVia, pSrc, &offset);
        if (!isAGP && !viaExaIsOffscreen(pSrc))
            return FALSE;
        if (!v3d->setTexture(v3d, curTex, offset,
                             exaGetPixmapPitch(pSrc), pVia->nPOT[curTex],
                             1 << width, 1 << height, pSrcPicture->format,
                             via_repeat, via_repeat, srcMode, isAGP)) {
            return FALSE;
        }
        curTex++;
    }

    if (pMaskPicture && !pVia->maskP) {
        offset = exaGetPixmapOffset(pMask);
        isAGP = viaIsAGP(pVia, pMask, &offset);
        if (!isAGP && !viaExaIsOffscreen(pMask))
            return FALSE;
        viaOrder(pMask->drawable.width, &width);
        viaOrder(pMask->drawable.height, &height);
        if (!v3d->setTexture(v3d, curTex, offset,
                             exaGetPixmapPitch(pMask), pVia->nPOT[curTex],
                             1 << width, 1 << height, pMaskPicture->format,
                             via_repeat, via_repeat,
                             ((pMaskPicture->componentAlpha)
                              ? via_comp_mask : via_mask), isAGP)) {
            return FALSE;
        }
        curTex++;
    }

    v3d->setFlags(v3d, curTex, FALSE, TRUE, TRUE);
    v3d->emitState(v3d, &pVia->cb, viaCheckUpload(pScrn, v3d));
    v3d->emitClipRect(v3d, &pVia->cb, 0, 0, pDst->drawable.width,
                      pDst->drawable.height);

    return TRUE;
}

void
viaExaComposite_H6(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY,
                    int dstX, int dstY, int width, int height)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    Via3DState *v3d = &pVia->v3d;
    CARD32 col;

    if (pVia->maskP) {
        viaPixelARGB8888(pVia->maskFormat, pVia->maskP, &col);
        v3d->setTexBlendCol(v3d, 0, pVia->componentAlpha, col);
    }
    if (pVia->srcP) {
        viaPixelARGB8888(pVia->srcFormat, pVia->srcP, &col);
        v3d->setDrawing(v3d, 0x0c, 0xFFFFFFFF, col & 0x00FFFFFF, col >> 24);
        srcX = maskX;
        srcY = maskY;
    }

    if (pVia->maskP || pVia->srcP)
        v3d->emitState(v3d, &pVia->cb, viaCheckUpload(pScrn, v3d));

    v3d->emitQuad(v3d, &pVia->cb, dstX, dstY, srcX, srcY, maskX, maskY,
                  width, height);
}
