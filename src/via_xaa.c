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
#include "xaalocal.h"
#include "xaarop.h"
#include "miline.h"

#include "via_driver.h"
#include "via_regs.h"
#include "via_dmabuffer.h"

#ifdef X_HAVE_XAAGETROP
#define VIAACCELPATTERNROP(vRop) (XAAGetPatternROP(vRop) << 24)
#define VIAACCELCOPYROP(vRop) (XAAGetCopyROP(vRop) << 24)
#else
#define VIAACCELPATTERNROP(vRop) (XAAPatternROP[vRop] << 24)
#define VIAACCELCOPYROP(vRop) (XAACopyROP[vRop] << 24)
#endif

enum VIA_2D_Regs {
    GECMD,
    GEMODE,
    GESTATUS,
    SRCPOS,
    DSTPOS,
    LINE_K1K2,
    LINE_XY,
    LINE_ERROR,
    DIMENSION,
    PATADDR,
    FGCOLOR,
    DSTCOLORKEY,
    BGCOLOR,
    SRCCOLORKEY,
    CLIPTL,
    CLIPBR,
    OFFSET,
    KEYCONTROL,
    SRCBASE,
    DSTBASE,
    PITCH,
    MONOPAT0,
    MONOPAT1,
    COLORPAT,
    MONOPATFGC,
    MONOPATBGC
};

/* register offsets for old 2D core */
static const unsigned via_2d_regs[] = {
    [GECMD]             = VIA_REG_GECMD,
    [GEMODE]            = VIA_REG_GEMODE,
    [GESTATUS]          = VIA_REG_GESTATUS,
    [SRCPOS]            = VIA_REG_SRCPOS,
    [DSTPOS]            = VIA_REG_DSTPOS,
    [LINE_K1K2]         = VIA_REG_LINE_K1K2,
    [LINE_XY]           = VIA_REG_LINE_XY,
    [LINE_ERROR]        = VIA_REG_LINE_ERROR,
    [DIMENSION]         = VIA_REG_DIMENSION,
    [PATADDR]           = VIA_REG_PATADDR,
    [FGCOLOR]           = VIA_REG_FGCOLOR,
    [DSTCOLORKEY]       = VIA_REG_DSTCOLORKEY,
    [BGCOLOR]           = VIA_REG_BGCOLOR,
    [SRCCOLORKEY]       = VIA_REG_SRCCOLORKEY,
    [CLIPTL]            = VIA_REG_CLIPTL,
    [CLIPBR]            = VIA_REG_CLIPBR,
    [KEYCONTROL]        = VIA_REG_KEYCONTROL,
    [SRCBASE]           = VIA_REG_SRCBASE,
    [DSTBASE]           = VIA_REG_DSTBASE,
    [PITCH]             = VIA_REG_PITCH,
    [MONOPAT0]          = VIA_REG_MONOPAT0,
    [MONOPAT1]          = VIA_REG_MONOPAT1,
    [COLORPAT]          = VIA_REG_COLORPAT,
    [MONOPATFGC]        = VIA_REG_FGCOLOR,
    [MONOPATBGC]        = VIA_REG_BGCOLOR
};

/* register offsets for new 2D core (M1 in VT3353 == VX800) */
static const unsigned via_2d_regs_m1[] = {
    [GECMD]             = VIA_REG_GECMD_M1,
    [GEMODE]            = VIA_REG_GEMODE_M1,
    [GESTATUS]          = VIA_REG_GESTATUS_M1,
    [SRCPOS]            = VIA_REG_SRCPOS_M1,
    [DSTPOS]            = VIA_REG_DSTPOS_M1,
    [LINE_K1K2]         = VIA_REG_LINE_K1K2_M1,
    [LINE_XY]           = VIA_REG_LINE_XY_M1,
    [LINE_ERROR]        = VIA_REG_LINE_ERROR_M1,
    [DIMENSION]         = VIA_REG_DIMENSION_M1,
    [PATADDR]           = VIA_REG_PATADDR_M1,
    [FGCOLOR]           = VIA_REG_FGCOLOR_M1,
    [DSTCOLORKEY]       = VIA_REG_DSTCOLORKEY_M1,
    [BGCOLOR]           = VIA_REG_BGCOLOR_M1,
    [SRCCOLORKEY]       = VIA_REG_SRCCOLORKEY_M1,
    [CLIPTL]            = VIA_REG_CLIPTL_M1,
    [CLIPBR]            = VIA_REG_CLIPBR_M1,
    [KEYCONTROL]        = VIA_REG_KEYCONTROL_M1,
    [SRCBASE]           = VIA_REG_SRCBASE_M1,
    [DSTBASE]           = VIA_REG_DSTBASE_M1,
    [PITCH]             = VIA_REG_PITCH_M1,
    [MONOPAT0]          = VIA_REG_MONOPAT0_M1,
    [MONOPAT1]          = VIA_REG_MONOPAT1_M1,
    [COLORPAT]          = VIA_REG_COLORPAT_M1,
    [MONOPATFGC]        = VIA_REG_MONOPATFGC_M1,
    [MONOPATBGC]        = VIA_REG_MONOPATBGC_M1
};

#define VIA_REG(pVia, name)	(pVia)->TwodRegs[name]

/*
 * Switch 2D state clipping on.
 */
static void
viaSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    tdc->clipping = TRUE;
    tdc->clipX1 = (x1 & 0xFFFF);
    tdc->clipY1 = y1;
    tdc->clipX2 = (x2 & 0xFFFF);
    tdc->clipY2 = y2;
}

/*
 * Switch 2D state clipping off.
 */
static void
viaDisableClipping(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    tdc->clipping = FALSE;
}

/*
 * This is a small helper to wrap around a PITCH register write
 * to deal with the subtle differences of M1 and old 2D engine
 */
static void
viaPitchHelper(VIAPtr pVia, unsigned dstPitch, unsigned srcPitch)
{
    unsigned val = (dstPitch >> 3) << 16 | (srcPitch >> 3);
    RING_VARS;

    if (pVia->Chipset != VIA_VX800 &&
        pVia->Chipset != VIA_VX855 &&
        pVia->Chipset != VIA_VX900) {
        val |= VIA_PITCH_ENABLE;
    }
    OUT_RING_H1(VIA_REG(pVia, PITCH), val);
}

/*
 * Emit clipping borders to the command buffer and update the 2D context
 * current command with clipping info.
 */
static int
viaAccelClippingHelper(VIAPtr pVia, int refY)
{
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    if (tdc->clipping) {
        refY = (refY < tdc->clipY1) ? refY : tdc->clipY1;
        tdc->cmd |= VIA_GEC_CLIP_ENABLE;
        BEGIN_RING(4);
        OUT_RING_H1(VIA_REG(pVia, CLIPTL),
                    ((tdc->clipY1 - refY) << 16) | tdc->clipX1);
        OUT_RING_H1(VIA_REG(pVia, CLIPBR),
		    ((tdc->clipY2 - refY) << 16) | tdc->clipX2);
    } else {
        tdc->cmd &= ~VIA_GEC_CLIP_ENABLE;
    }
    return refY;
}

/*
 * Emit a solid blit operation to the command buffer.
 */
static void
viaAccelSolidHelper(VIAPtr pVia, int x, int y, int w, int h,
                    unsigned fbBase, CARD32 mode, unsigned pitch,
                    CARD32 fg, CARD32 cmd)
{
    RING_VARS;

    BEGIN_RING(14);
    OUT_RING_H1(VIA_REG(pVia, GEMODE), mode);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), fbBase >> 3);
    viaPitchHelper(pVia, pitch, 0);
    OUT_RING_H1(VIA_REG(pVia, DSTPOS), (y << 16) | (x & 0xFFFF));
    OUT_RING_H1(VIA_REG(pVia, DIMENSION), ((h - 1) << 16) | (w - 1));
    OUT_RING_H1(VIA_REG(pVia, MONOPATFGC), fg);
    OUT_RING_H1(VIA_REG(pVia, GECMD), cmd);
}

/*
 * Check if we can use a planeMask and update the 2D context accordingly.
 */
static Bool
viaAccelPlaneMaskHelper(ViaTwodContext * tdc, CARD32 planeMask)
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
viaAccelTransparentHelper(VIAPtr pVia, CARD32 keyControl,
                          CARD32 transColor, Bool usePlaneMask)
{
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    tdc->keyControl &= ((usePlaneMask) ? 0xF0000000 : 0x00000000);
    tdc->keyControl |= (keyControl & 0x0FFFFFFF);
    BEGIN_RING(4);
    OUT_RING_H1(VIA_REG(pVia, KEYCONTROL), tdc->keyControl);
    if (keyControl) {
        OUT_RING_H1(VIA_REG(pVia, SRCCOLORKEY), transColor);
    }
}

/*
 * Emit a copy blit operation to the command buffer.
 */
static void
viaAccelCopyHelper(VIAPtr pVia, int xs, int ys, int xd, int yd,
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
    OUT_RING_H1(VIA_REG(pVia, GEMODE), mode);
    OUT_RING_H1(VIA_REG(pVia, SRCBASE), srcFbBase >> 3);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), dstFbBase >> 3);
    viaPitchHelper(pVia, dstPitch, srcPitch);
    OUT_RING_H1(VIA_REG(pVia, SRCPOS), (ys << 16) | (xs & 0xFFFF));
    OUT_RING_H1(VIA_REG(pVia, DSTPOS), (yd << 16) | (xd & 0xFFFF));
    OUT_RING_H1(VIA_REG(pVia, DIMENSION), ((h - 1) << 16) | (w - 1));
    OUT_RING_H1(VIA_REG(pVia, GECMD), cmd);
}

/*
 * XAA functions. Note that the blitter limit of 2047 lines has been worked
 * around by adding min(y1, y2, clipping y) * stride to the offset (which is
 * recommended by VIA docs).  The y values (including clipping) must be
 * subtracted accordingly.
 */
static void
viaSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
                              unsigned planemask, int trans_color)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 cmd;
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    cmd = VIA_GEC_BLT | VIAACCELCOPYROP(rop);

    if (xdir < 0)
        cmd |= VIA_GEC_DECX;

    if (ydir < 0)
        cmd |= VIA_GEC_DECY;

    tdc->cmd = cmd;
    viaAccelTransparentHelper(pVia, (trans_color != -1) ? 0x4000 : 0x0000,
                              trans_color, FALSE);
    ADVANCE_RING;
}

static void
viaSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
                                int x2, int y2, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    RING_VARS;

    if (!w || !h)
        return;

    sub = viaAccelClippingHelper(pVia, y2);
    viaAccelCopyHelper(pVia, x1, 0, x2, y2 - sub, w, h,
                       pScrn->fbOffset + pVia->Bpl * y1,
                       pScrn->fbOffset + pVia->Bpl * sub,
                       tdc->mode, pVia->Bpl, pVia->Bpl, tdc->cmd);
    ADVANCE_RING;
}

/*
 * SetupForSolidFill is also called to set up for lines.
 */
static void
viaSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, unsigned planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);
    tdc->fgColor = color;
    viaAccelTransparentHelper(pVia, 0x00, 0x00, FALSE);
    ADVANCE_RING;
}

static void
viaSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    RING_VARS;

    if (!w || !h)
        return;

    sub = viaAccelClippingHelper(pVia, y);
    viaAccelSolidHelper(pVia, x, y - sub, w, h,
                        pScrn->fbOffset + pVia->Bpl * sub, tdc->mode, pVia->Bpl,
                        tdc->fgColor, tdc->cmd);
    ADVANCE_RING;
}

/*
 * Original VIA comment:
 * The meaning of the two pattern parameters to Setup & Subsequent for
 * Mono8x8Patterns varies depending on the flag bits.  We specify
 * HW_PROGRAMMED_BITS, which means our hardware can handle 8x8 patterns
 * without caching in the frame buffer.  Thus, Setup gets the pattern bits.
 * There is no way with BCI to rotate an 8x8 pattern, so we do NOT specify
 * HW_PROGRAMMED_ORIGIN.  XAA wil rotate it for us and pass the rotated
 * pattern to both Setup and Subsequent.  If we DID specify PROGRAMMED_ORIGIN,
 * then Setup would get the unrotated pattern, and Subsequent gets the
 * origin values.
 */
static void
viaSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int pattern0, int pattern1,
                              int fg, int bg, int rop, unsigned planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int cmd;
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    cmd = VIA_GEC_BLT | VIA_GEC_PAT_REG | VIA_GEC_PAT_MONO |
            VIAACCELPATTERNROP(rop);

    if (bg == -1) {
        cmd |= VIA_GEC_MPAT_TRANS;
    }

    tdc->cmd = cmd;
    tdc->fgColor = fg;
    tdc->bgColor = bg;
    tdc->pattern0 = pattern0;
    tdc->pattern1 = pattern1;
    viaAccelTransparentHelper(pVia, 0x00, 0x00, FALSE);
    ADVANCE_RING;
}

static void
viaSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patOffx,
                                    int patOffy, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 patOffset;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    RING_VARS;

    if (!w || !h)
        return;

    patOffset = ((patOffy & 0x7) << 29) | ((patOffx & 0x7) << 26);
    sub = viaAccelClippingHelper(pVia, y);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING(22);
    OUT_RING_H1(VIA_REG(pVia, GEMODE), tdc->mode);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), dstBase >> 3);
    viaPitchHelper(pVia, pVia->Bpl, 0);
    OUT_RING_H1(VIA_REG(pVia, DSTPOS), ((y - sub) << 16) | (x & 0xFFFF));
    OUT_RING_H1(VIA_REG(pVia, DIMENSION), (((h - 1) << 16) | (w - 1)));
    OUT_RING_H1(VIA_REG(pVia, PATADDR), patOffset);
    OUT_RING_H1(VIA_REG(pVia, MONOPATFGC), tdc->fgColor);
    OUT_RING_H1(VIA_REG(pVia, MONOPATBGC), tdc->bgColor);
    OUT_RING_H1(VIA_REG(pVia, MONOPAT0), tdc->pattern0);
    OUT_RING_H1(VIA_REG(pVia, MONOPAT1), tdc->pattern1);
    OUT_RING_H1(VIA_REG(pVia, GECMD), tdc->cmd);
    ADVANCE_RING;
}

static void
viaSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny,
                               int rop, unsigned planemask, int trans_color)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    tdc->cmd = VIA_GEC_BLT | VIAACCELPATTERNROP(rop);
    tdc->patternAddr = (patternx * pVia->Bpp + patterny * pVia->Bpl);
    viaAccelTransparentHelper(pVia, (trans_color != -1) ? 0x4000 : 0x0000,
                              trans_color, FALSE);
    ADVANCE_RING;
}

static void
viaSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patOffx,
                                     int patOffy, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 patAddr;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    RING_VARS;

    if (!w || !h)
        return;

    patAddr = (tdc->patternAddr >> 3) |
            ((patOffy & 0x7) << 29) | ((patOffx & 0x7) << 26);
    sub = viaAccelClippingHelper(pVia, y);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING(14);
    OUT_RING_H1(VIA_REG(pVia, GEMODE), tdc->mode);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), dstBase >> 3);
    viaPitchHelper(pVia, pVia->Bpl, 0);
    OUT_RING_H1(VIA_REG(pVia, DSTPOS), ((y - sub) << 16) | (x & 0xFFFF));
    OUT_RING_H1(VIA_REG(pVia, DIMENSION), (((h - 1) << 16) | (w - 1)));
    OUT_RING_H1(VIA_REG(pVia, PATADDR), patAddr);
    OUT_RING_H1(VIA_REG(pVia, GECMD), tdc->cmd);
    ADVANCE_RING;
}

/*
 * CPU-to-screen functions cannot use AGP due to complicated syncing.
 * Therefore the command buffer is flushed before new command emissions, and
 * viaFluchPCI() is called explicitly instead of cb->flushFunc() at the end of
 * each CPU-to-screen function.  Should the buffer get completely filled again
 * by a CPU-to-screen command emission, a horrible error will occur.
 */
static void
viaSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg, int bg,
                                      int rop, unsigned planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int cmd;
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | VIA_GEC_SRC_MONO |
            VIAACCELCOPYROP(rop);

    if (bg == -1) {
        cmd |= VIA_GEC_MSRC_TRANS;
    }

    tdc->cmd = cmd;
    tdc->fgColor = fg;
    tdc->bgColor = bg;

    viaAccelTransparentHelper(pVia, 0x0, 0x0, FALSE);

    ADVANCE_RING;
}

static void
viaSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
                                                int y, int w, int h,
                                                int skipleft)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    RING_VARS;

    if (skipleft) {
        viaSetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1),
                                (y + h - 1));
    }

    sub = viaAccelClippingHelper(pVia, y);
    BEGIN_RING(4);
    OUT_RING_H1(VIA_REG(pVia, BGCOLOR), tdc->bgColor);
    OUT_RING_H1(VIA_REG(pVia, FGCOLOR), tdc->fgColor);
    viaAccelCopyHelper(pVia, 0, 0, x, y - sub, w, h, 0,
                       pScrn->fbOffset + sub * pVia->Bpl, tdc->mode,
                       pVia->Bpl, pVia->Bpl, tdc->cmd);

    ADVANCE_RING;
    viaDisableClipping(pScrn);
}

static void
viaSetupForImageWrite(ScrnInfoPtr pScrn, int rop, unsigned planemask,
                      int trans_color, int bpp, int depth)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | VIAACCELCOPYROP(rop);
    viaAccelTransparentHelper(pVia, (trans_color != -1) ? 0x4000 : 0x0000,
                              trans_color, FALSE);
    ADVANCE_RING;
}

static void
viaSubsequentImageWriteRect(ScrnInfoPtr pScrn, int x, int y, int w, int h,
                            int skipleft)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    RING_VARS;

    if (skipleft) {
        viaSetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1),
                                (y + h - 1));
    }

    sub = viaAccelClippingHelper(pVia, y);
    viaAccelCopyHelper(pVia, 0, 0, x, y - sub, w, h, 0,
                       pScrn->fbOffset + pVia->Bpl * sub, tdc->mode,
                       pVia->Bpl, pVia->Bpl, tdc->cmd);

    ADVANCE_RING;
    viaDisableClipping(pScrn);
}

static void
viaSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
                     unsigned int planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    viaAccelTransparentHelper(pVia, 0x00, 0x00, FALSE);
    tdc->cmd = VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);
    tdc->fgColor = color;
    tdc->dashed = FALSE;

    BEGIN_RING(6);
    OUT_RING_H1(VIA_REG(pVia, GEMODE), tdc->mode);
    OUT_RING_H1(VIA_REG(pVia, MONOPAT0), 0xFF);
    OUT_RING_H1(VIA_REG(pVia, MONOPATFGC), tdc->fgColor);
    ADVANCE_RING;
}

static void
viaSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
                               int x2, int y2, int flags)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int dx, dy, cmd, tmp, error = 1;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    RING_VARS;

    sub = viaAccelClippingHelper(pVia, (y1 < y2) ? y1 : y2);
    cmd = tdc->cmd | VIA_GEC_LINE;

    dx = x2 - x1;
    if (dx < 0) {
        dx = -dx;
        cmd |= VIA_GEC_DECX;    /* line will be drawn from right */
        error = 0;
    }

    dy = y2 - y1;
    if (dy < 0) {
        dy = -dy;
        cmd |= VIA_GEC_DECY;    /* line will be drawn from bottom */
    }

    if (dy > dx) {
        tmp = dy;
        dy = dx;
        dx = tmp;               /* Swap 'dx' and 'dy' */
        cmd |= VIA_GEC_Y_MAJOR; /* Y major line */
    }

    if (flags & OMIT_LAST) {
        cmd |= VIA_GEC_LASTPIXEL_OFF;
    }

    dstBase = pScrn->fbOffset + sub * pVia->Bpl;
    y1 -= sub;
    y2 -= sub;

    BEGIN_RING(14);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), dstBase >> 3);
    viaPitchHelper(pVia, pVia->Bpl, 0);

    /*
     * major = 2*dmaj, minor = 2*dmin, err = -dmaj - ((bias >> octant) & 1)
     * K1 = 2*dmin K2 = 2*(dmin - dmax)
     * Error Term = (StartX<EndX) ? (2*dmin - dmax - 1) : (2*(dmin - dmax))
     */

    OUT_RING_H1(VIA_REG(pVia, LINE_K1K2),
                ((((dy << 1) & 0x3fff) << 16) | (((dy - dx) << 1) & 0x3fff)));
    OUT_RING_H1(VIA_REG(pVia, LINE_XY), ((y1 << 16) | (x1 & 0xFFFF)));
    OUT_RING_H1(VIA_REG(pVia, DIMENSION), dx);
    OUT_RING_H1(VIA_REG(pVia, LINE_ERROR),
                (((dy << 1) - dx - error) & 0x3fff) |
                ((tdc->dashed) ? 0xFF0000 : 0));
    OUT_RING_H1(VIA_REG(pVia, GECMD), cmd);
    ADVANCE_RING;
}

static void
viaSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len, int dir)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    RING_VARS;

    sub = viaAccelClippingHelper(pVia, y);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING(10);
    OUT_RING_H1(VIA_REG(pVia, DSTBASE), dstBase >> 3);
    viaPitchHelper(pVia, pVia->Bpl, 0);

    if (dir == DEGREES_0) {
        OUT_RING_H1(VIA_REG(pVia, DSTPOS), ((y - sub) << 16) | (x & 0xFFFF));
        OUT_RING_H1(VIA_REG(pVia, DIMENSION), (len - 1));
        OUT_RING_H1(VIA_REG(pVia, GECMD), tdc->cmd | VIA_GEC_BLT);
    } else {
        OUT_RING_H1(VIA_REG(pVia, DSTPOS), ((y - sub) << 16) | (x & 0xFFFF));
        OUT_RING_H1(VIA_REG(pVia, DIMENSION), ((len - 1) << 16));
        OUT_RING_H1(VIA_REG(pVia, GECMD), tdc->cmd | VIA_GEC_BLT);
    }
    ADVANCE_RING;
}

static void
viaSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
                      unsigned int planemask, int length,
                      unsigned char *pattern)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int cmd;
    CARD32 pat = *(CARD32 *) pattern;
    ViaTwodContext *tdc = &pVia->td;

    RING_VARS;

    viaAccelTransparentHelper(pVia, 0x00, 0x00, FALSE);
    cmd = VIA_GEC_LINE | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);

    if (bg == -1) {
        cmd |= VIA_GEC_MPAT_TRANS;
    }

    tdc->cmd = cmd;
    tdc->fgColor = fg;
    tdc->bgColor = bg;

    switch (length) {
        case 2:
            pat |= pat << 2;    /* fall through */
        case 4:
            pat |= pat << 4;    /* fall through */
        case 8:
            pat |= pat << 8;    /* fall through */
        case 16:
            pat |= pat << 16;
    }

    tdc->pattern0 = pat;
    tdc->dashed = TRUE;

    BEGIN_RING(8);
    OUT_RING_H1(VIA_REG(pVia, GEMODE), tdc->mode);
    OUT_RING_H1(VIA_REG(pVia, MONOPATFGC), tdc->fgColor);
    OUT_RING_H1(VIA_REG(pVia, MONOPATBGC), tdc->bgColor);
    OUT_RING_H1(VIA_REG(pVia, MONOPAT0), tdc->pattern0);
    ADVANCE_RING;
}

static void
viaSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2,
                                int y2, int flags, int phase)
{
    viaSubsequentSolidTwoPointLine(pScrn, x1, y1, x2, y2, flags);
}

Bool
viaInitXAA(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    XAAInfoRecPtr xaaptr;

    if (!(xaaptr = pVia->AccelInfoRec = XAACreateInfoRec()))
        return FALSE;

    /* General acceleration flags. */
    xaaptr->Flags = (PIXMAP_CACHE |
                     OFFSCREEN_PIXMAPS |
                     LINEAR_FRAMEBUFFER |
                     MICROSOFT_ZERO_LINE_BIAS | 0);

    if (pScrn->bitsPerPixel == 8)
        xaaptr->CachePixelGranularity = 128;

    xaaptr->SetClippingRectangle = viaSetClippingRectangle;
    xaaptr->DisableClipping = viaDisableClipping;
    xaaptr->ClippingFlags = (HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
                             HARDWARE_CLIP_MONO_8x8_FILL |
                             HARDWARE_CLIP_COLOR_8x8_FILL |
                             HARDWARE_CLIP_SCREEN_TO_SCREEN_COLOR_EXPAND | 0);

    if (pVia->Chipset != VIA_VX800 &&
        pVia->Chipset != VIA_VX855 &&
        pVia->Chipset != VIA_VX900)
		xaaptr->ClippingFlags |= (HARDWARE_CLIP_SOLID_FILL |
                                  HARDWARE_CLIP_SOLID_LINE |
                                  HARDWARE_CLIP_DASHED_LINE);

    xaaptr->Sync = viaAccelSync;

    /* ScreenToScreen copies */
    xaaptr->SetupForScreenToScreenCopy = viaSetupForScreenToScreenCopy;
    xaaptr->SubsequentScreenToScreenCopy = viaSubsequentScreenToScreenCopy;
    xaaptr->ScreenToScreenCopyFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* Solid filled rectangles */
    xaaptr->SetupForSolidFill = viaSetupForSolidFill;
    xaaptr->SubsequentSolidFillRect = viaSubsequentSolidFillRect;
    xaaptr->SolidFillFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* Mono 8x8 pattern fills */
    xaaptr->SetupForMono8x8PatternFill = viaSetupForMono8x8PatternFill;
    xaaptr->SubsequentMono8x8PatternFillRect =
            viaSubsequentMono8x8PatternFillRect;
    xaaptr->Mono8x8PatternFillFlags = (NO_PLANEMASK |
                                       HARDWARE_PATTERN_PROGRAMMED_BITS |
                                       HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
                                       BIT_ORDER_IN_BYTE_MSBFIRST | 0);

    /* Color 8x8 pattern fills */
    xaaptr->SetupForColor8x8PatternFill = viaSetupForColor8x8PatternFill;
    xaaptr->SubsequentColor8x8PatternFillRect =
            viaSubsequentColor8x8PatternFillRect;
    xaaptr->Color8x8PatternFillFlags = (NO_PLANEMASK |
                                        NO_TRANSPARENCY |
                                        HARDWARE_PATTERN_PROGRAMMED_BITS |
                                        HARDWARE_PATTERN_PROGRAMMED_ORIGIN | 0);

    /* Solid lines */
    xaaptr->SetupForSolidLine = viaSetupForSolidLine;
    xaaptr->SubsequentSolidTwoPointLine = viaSubsequentSolidTwoPointLine;
    xaaptr->SubsequentSolidHorVertLine = viaSubsequentSolidHorVertLine;
    xaaptr->SolidBresenhamLineErrorTermBits = 14;
    xaaptr->SolidLineFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    /* Dashed line */
    xaaptr->SetupForDashedLine = viaSetupForDashedLine;
    xaaptr->SubsequentDashedTwoPointLine = viaSubsequentDashedTwoPointLine;
    xaaptr->DashPatternMaxLength = 8;
    xaaptr->DashedLineFlags = (NO_PLANEMASK |
                               ROP_NEEDS_SOURCE |
                               LINE_PATTERN_POWER_OF_2_ONLY |
                               LINE_PATTERN_MSBFIRST_LSBJUSTIFIED | 0);

    /* CPU to Screen color expansion */
    xaaptr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
						CPU_TRANSFER_PAD_DWORD |
						SCANLINE_PAD_DWORD |
						BIT_ORDER_IN_BYTE_MSBFIRST |
						LEFT_EDGE_CLIPPING |
						ROP_NEEDS_SOURCE | 0;

    xaaptr->SetupForScanlineCPUToScreenColorExpandFill =
            viaSetupForCPUToScreenColorExpandFill;
    xaaptr->SubsequentScanlineCPUToScreenColorExpandFill =
            viaSubsequentScanlineCPUToScreenColorExpandFill;
    xaaptr->ColorExpandBase = pVia->BltBase;
    if (pVia->Chipset == VIA_VX800 ||
        pVia->Chipset == VIA_VX855 ||
        pVia->Chipset == VIA_VX900)
        xaaptr->ColorExpandRange = VIA_MMIO_BLTSIZE;
    else
        xaaptr->ColorExpandRange = (64 * 1024);

    /* ImageWrite */
    xaaptr->ImageWriteFlags = (NO_PLANEMASK |
								CPU_TRANSFER_PAD_DWORD |
								SCANLINE_PAD_DWORD |
								BIT_ORDER_IN_BYTE_MSBFIRST |
								LEFT_EDGE_CLIPPING |
								ROP_NEEDS_SOURCE |
								NO_GXCOPY | 0);

    /*
     * Most Unichromes are much faster using processor-to-framebuffer writes
     * than when using the 2D engine for this.
     * test with "x11perf -shmput500"
     * Example: K8M890 chipset; with GPU=86.3/sec; without GPU=132.0/sec
     * TODO Check speed for other chipsets
     */

    xaaptr->SetupForImageWrite = viaSetupForImageWrite;
    xaaptr->SubsequentImageWriteRect = viaSubsequentImageWriteRect;
    xaaptr->ImageWriteBase = pVia->BltBase;

    if (pVia->Chipset == VIA_VX800 ||
        pVia->Chipset == VIA_VX855 ||
        pVia->Chipset == VIA_VX900)
        xaaptr->ImageWriteRange = VIA_MMIO_BLTSIZE;
    else
        xaaptr->ImageWriteRange = (64 * 1024);

    return XAAInit(pScreen, xaaptr);
}
