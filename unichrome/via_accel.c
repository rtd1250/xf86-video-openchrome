/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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
 *
 * Mostly rewritten and modified for EXA support by Thomas Hellstrom 2005.
 */

/*************************************************************************
 *
 *  File:       via_accel.c
 *  Content:    2D acceleration function for VIA/S3G UniChrome
 *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xarch.h>
#include "xaalocal.h"
#include "xaarop.h"
#include "miline.h"

#include "via_driver.h"
#include "via_regs.h"
#include "via_id.h"
#include "via_dmabuffer.h"

#ifdef X_HAVE_XAAGETROP
#define VIAACCELPATTERNROP(vRop) (XAAGetPatternROP(vRop) << 24)
#define VIAACCELCOPYROP(vRop) (XAAGetCopyROP(vRop) << 24)
#else
#define VIAACCELPATTERNROP(vRop) (XAAPatternROP[vRop] << 24)
#define VIAACCELCOPYROP(vRop) (XAACopyROP[vRop] << 24)
#endif

/*
 * Use PCI MMIO to flush the command buffer. When AGP DMA is not available.
 */

void
viaFlushPCI(ViaCommandBuffer * buf)
{
    unsigned size = buf->pos >> 1;
    int i;
    register CARD32 *bp = buf->buf;
    unsigned loop = 0;
    register unsigned offset;
    register unsigned value;
    VIAPtr pVia = VIAPTR(buf->pScrn);

    /*
     * Not doing this wait will probably stall the processor
     * for an unacceptable amount of time in VIASETREG while other high
     * priority interrupts may be pending.
     */

    while (!(VIAGETREG(VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY)
	&& (loop++ < MAXLOOP)) ;
    while ((VIAGETREG(VIA_REG_STATUS) & (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY))
	&& (loop++ < MAXLOOP)) ;

    for (i = 0; i < size; ++i) {
	offset = (*bp++ & 0x0FFFFFFF) << 2;
	value = *bp++;
	VIASETREG(offset, value);
    }
    buf->pos = 0;
}

/*
 * Use AGP DMA to flush the command buffer. If it fails it skips the current
 * command buffer and reverts to PCI MMIO until the server is reset.
 */

#ifdef XF86DRI
static void
viaFlushDRIEnabled(ViaCommandBuffer * cb)
{
    ScrnInfoPtr pScrn = cb->pScrn;
    VIAPtr pVia = VIAPTR(pScrn);
    char *tmp = (char *)cb->buf;
    int tmpSize = cb->pos * sizeof(CARD32);
    drm_via_cmdbuffer_t b;

    if (pVia->agpDMA) {
	while (tmpSize > 0) {
	    b.size = (tmpSize > VIA_DMASIZE) ? VIA_DMASIZE : tmpSize;
	    tmpSize -= b.size;
	    b.buf = tmp;
	    tmp += b.size;
	    if (drmCommandWrite(pVia->drmFD, DRM_VIA_CMDBUFFER, &b,
		    sizeof(b))) {
		ErrorF("AGP DMA command submission failed.\n");
		pVia->agpDMA = FALSE;
		cb->flushFunc = viaFlushPCI;
		return;
	    }
	}
	cb->pos = 0;
    } else {
	cb->flushFunc = viaFlushPCI;
	viaFlushPCI(cb);
    }
}
#endif

/*
 * Initialize a command buffer. Some fields are currently not used since they
 * are intended for Unichrome Pro group A video commands.
 */

int
viaSetupCBuffer(ScrnInfoPtr pScrn, ViaCommandBuffer * buf, unsigned size)
{
    buf->pScrn = pScrn;
    buf->bufSize = ((size == 0) ? VIA_DMASIZE : size) >> 2;
    buf->buf = (CARD32 *) xcalloc(buf->bufSize, sizeof(CARD32));
    if (!buf->buf)
	return BadAlloc;
    buf->waitFlags = 0;
    buf->pos = 0;
    buf->mode = 0;
    buf->header_start = 0;
    buf->rindex = 0;
#ifdef XF86DRI
    buf->flushFunc = viaFlushDRIEnabled;
#else
    buf->flushFunc = viaFlushPCI;
#endif
    return Success;
}

/*
 * Free resources associated with a command buffer.
 */

void
viaTearDownCBuffer(ViaCommandBuffer * buf)
{
    if (buf && buf->buf)
	xfree(buf->buf);
    buf->buf = NULL;
}

/*
 * Leftover from VIAs code.
 */

static void
viaInitAgp(VIAPtr pVia)
{
    VIASETREG(VIA_REG_TRANSET, 0x00100000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00333004);
    VIASETREG(VIA_REG_TRANSPACE, 0x60000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x61000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x62000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x63000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x64000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x7D000000);

    VIASETREG(VIA_REG_TRANSET, 0xfe020000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000000);
}

/*
 * Initialize the virtual command queue. Header 2 commands can be put
 * in this queue for buffering. AFAIK it doesn't handle Header 1 
 * commands, which is really a pity, since it has to be idled before
 * issuing a H1 command.
 */

static void
viaEnableVQ(VIAPtr pVia)
{
    CARD32
	vqStartAddr = pVia->VQStart,
	vqEndAddr = pVia->VQEnd,
	vqStartL = 0x50000000 | (vqStartAddr & 0xFFFFFF),
	vqEndL = 0x51000000 | (vqEndAddr & 0xFFFFFF),
	vqStartEndH = 0x52000000 | ((vqStartAddr & 0xFF000000) >> 24) |
	((vqEndAddr & 0xFF000000) >> 16),
	vqLen = 0x53000000 | (VIA_VQ_SIZE >> 3);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE, 0x080003fe);
    VIASETREG(VIA_REG_TRANSPACE, 0x0a00027c);
    VIASETREG(VIA_REG_TRANSPACE, 0x0b000260);
    VIASETREG(VIA_REG_TRANSPACE, 0x0c000274);
    VIASETREG(VIA_REG_TRANSPACE, 0x0d000264);
    VIASETREG(VIA_REG_TRANSPACE, 0x0e000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x0f000020);
    VIASETREG(VIA_REG_TRANSPACE, 0x1000027e);
    VIASETREG(VIA_REG_TRANSPACE, 0x110002fe);
    VIASETREG(VIA_REG_TRANSPACE, 0x200f0060);

    VIASETREG(VIA_REG_TRANSPACE, 0x00000006);
    VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
    VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x45080c04);
    VIASETREG(VIA_REG_TRANSPACE, 0x46800408);

    VIASETREG(VIA_REG_TRANSPACE, vqStartEndH);
    VIASETREG(VIA_REG_TRANSPACE, vqStartL);
    VIASETREG(VIA_REG_TRANSPACE, vqEndL);
    VIASETREG(VIA_REG_TRANSPACE, vqLen);
}

/*
 * Disable the virtual command queue.
 */

void
viaDisableVQ(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000004);
    VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
    VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x45080c04);
    VIASETREG(VIA_REG_TRANSPACE, 0x46800408);
}

/*
 * Update our 2D state (TwoDContext) with a new mode.
 */

static Bool
viaAccelSetMode(int bpp, ViaTwodContext * tdc)
{
    switch (bpp) {
    case 16:
	tdc->mode = VIA_GEM_16bpp;
	tdc->bytesPPShift = 1;
	return TRUE;
    case 32:
	tdc->mode = VIA_GEM_32bpp;
	tdc->bytesPPShift = 2;
	return TRUE;
    case 8:
	tdc->mode = VIA_GEM_8bpp;
	tdc->bytesPPShift = 0;
	return TRUE;
    default:
	tdc->bytesPPShift = 0;
	return FALSE;
    }
}

/*
 * Initialize the 2D engine and set the 2D context mode to the
 * current screen depth. Also enable the virtual queue. 
 */

void
viaInitialize2DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int i;

    /* 
     * init 2D engine regs to reset 2D engine 
     */

    for (i = 0x04; i < 0x44; i += 4) {
	VIASETREG(i, 0x0);
    }

    viaInitAgp(pVia);

    if (pVia->VQStart != 0) {
	viaEnableVQ(pVia);
    } else {
	viaDisableVQ(pScrn);
    }

    viaAccelSetMode(pScrn->bitsPerPixel, tdc);
}

/*
 * Wait for acceleration engines idle. An expensive way to sync.
 */

void
viaAccelSync(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int loop = 0;

    mem_barrier();

    while (!(VIAGETREG(VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY)
	&& (loop++ < MAXLOOP)) ;

    while ((VIAGETREG(VIA_REG_STATUS) &
	    (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)) &&
	(loop++ < MAXLOOP)) ;
}

/*
 * Set 2D state clipping on.
 */

static void
viaSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    tdc->clipping = TRUE;
    tdc->clipX1 = x1;
    tdc->clipY1 = y1;
    tdc->clipX2 = x2;
    tdc->clipY2 = y2;
}

/*
 * Set 2D state clipping off.
 */

static void
viaDisableClipping(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;

    tdc->clipping = FALSE;
}

/*
 * Emit clipping borders to the command buffer and update the 2D context
 * current command with clipping info.
 */

static int
viaAccelClippingHelper(ViaCommandBuffer * cb, int refY, ViaTwodContext * tdc)
{
    if (tdc->clipping) {
	refY = (refY < tdc->clipY1) ? refY : tdc->clipY1;
	tdc->cmd |= VIA_GEC_CLIP_ENABLE;
	BEGIN_RING_AGP(cb, 4);
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_CLIPTL),
	    ((tdc->clipY1 - refY) << 16) | tdc->clipX1);
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_CLIPBR),
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
viaAccelSolidHelper(ViaCommandBuffer * cb, int x, int y, int w, int h,
    unsigned fbBase, CARD32 mode, unsigned pitch, CARD32 fg, CARD32 cmd)
{
    BEGIN_RING_AGP(cb, 14);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), fbBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	(pitch >> 3) << 16);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), (y << 16) | x);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION),
	((h - 1) << 16) | (w - 1));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_FGCOLOR), fg);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), cmd);
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

	/*
	 * Masking doesn't work in 8bpp.
	 */

	if (modeMask == 0x0F) {
	    tdc->keyControl &= 0x0FFFFFFF;
	    return FALSE;
	}

	/*
	 * Translate the bit planemask to a byte planemask.
	 */

	for (i = 0; i < (1 << tdc->bytesPPShift); ++i) {
	    curByteMask = (0xFF << i);

	    if ((planeMask & curByteMask) == 0) {
		curMask |= (1 << i);
	    } else if ((planeMask & curByteMask) != curByteMask) {
		tdc->keyControl &= 0x0FFFFFFF;
		return FALSE;
	    }
	}

	tdc->keyControl = (tdc->keyControl & 0x0FFFFFFF) | (curMask << 28);
    }

    return TRUE;
}

/*
 * Emit transparency state and color to the command buffer.
 */

static void
viaAccelTransparentHelper(ViaTwodContext * tdc, ViaCommandBuffer * cb,
    CARD32 keyControl, CARD32 transColor, Bool usePlaneMask)
{
    tdc->keyControl &= ((usePlaneMask) ? 0xF0000000 : 0x00000000);
    tdc->keyControl |= (keyControl & 0x0FFFFFFF);
    BEGIN_RING_AGP(cb, 4);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_KEYCONTROL), tdc->keyControl);
    if (keyControl) {
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_SRCCOLORKEY), transColor);
    }
}

/*
 * Emit a copy blit operation to the command buffer.
 */

static void
viaAccelCopyHelper(ViaCommandBuffer * cb, int xs, int ys, int xd, int yd,
    int w, int h, unsigned srcFbBase, unsigned dstFbBase, CARD32 mode,
    unsigned srcPitch, unsigned dstPitch, CARD32 cmd)
{
    if (cmd & VIA_GEC_DECY) {
	ys += h - 1;
	yd += h - 1;
    }

    if (cmd & VIA_GEC_DECX) {
	xs += w - 1;
	xd += w - 1;
    }

    BEGIN_RING_AGP(cb, 16);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_SRCBASE), srcFbBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), dstFbBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	((dstPitch >> 3) << 16) | (srcPitch >> 3));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_SRCPOS), (ys << 16) | xs);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), (yd << 16) | xd);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION),
	((h - 1) << 16) | (w - 1));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), cmd);
}

/*
 * XAA functions. Note that the 2047 line blitter limit has been worked around by adding
 * min(y1, y2, clipping y) * stride to the offset (which is recommended by VIA docs).
 * The y values (including clipping) must be subtracted accordingly. 
 */

static void
viaSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
    unsigned planemask, int trans_color)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 cmd;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    cmd = VIA_GEC_BLT | VIAACCELCOPYROP(rop);

    if (xdir < 0)
	cmd |= VIA_GEC_DECX;

    if (ydir < 0)
	cmd |= VIA_GEC_DECY;

    tdc->cmd = cmd;
    viaAccelTransparentHelper(tdc, cb, (trans_color != -1) ? 0x4000 : 0x0000,
	trans_color, FALSE);
}

static void
viaSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
    int x2, int y2, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    if (!w || !h)
	return;

    sub = viaAccelClippingHelper(cb, y2, tdc);
    viaAccelCopyHelper(cb, x1, 0, x2, y2 - sub, w, h,
	pScrn->fbOffset + pVia->Bpl * y1, pScrn->fbOffset + pVia->Bpl * sub,
	tdc->mode, pVia->Bpl, pVia->Bpl, tdc->cmd);
    cb->flushFunc(cb);
}

/*
 * SetupForSolidFill is also called to set up for lines.
 */

static void
viaSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
    unsigned planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);
    tdc->fgColor = color;
    viaAccelTransparentHelper(tdc, cb, 0x00, 0x00, FALSE);
}

static void
viaSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    if (!w || !h)
	return;

    sub = viaAccelClippingHelper(cb, y, tdc);
    viaAccelSolidHelper(cb, x, y - sub, w, h,
	pScrn->fbOffset + pVia->Bpl * sub, tdc->mode, pVia->Bpl, tdc->fgColor,
	tdc->cmd);
    cb->flushFunc(cb);
}

/*
 * Original VIA comment:
 * The meaning of the two pattern paremeters to Setup & Subsequent for
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
    ViaCommandBuffer *cb = &pVia->cb;

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
    viaAccelTransparentHelper(tdc, cb, 0x00, 0x00, FALSE);

}

static void
viaSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patOffx,
    int patOffy, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 patOffset;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    if (!w || !h)
	return;

    patOffset = ((patOffy & 0x7) << 29) | ((patOffx & 0x7) << 26);
    sub = viaAccelClippingHelper(cb, y, tdc);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING_AGP(cb, 22);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), tdc->mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), dstBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	((pVia->Bpl >> 3) << 16));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), ((y - sub) << 16) | x);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION),
	(((h - 1) << 16) | (w - 1)));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PATADDR), patOffset);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_FGCOLOR), tdc->fgColor);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_BGCOLOR), tdc->bgColor);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_MONOPAT0), tdc->pattern0);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_MONOPAT1), tdc->pattern1);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), tdc->cmd);
    cb->flushFunc(cb);
}

static void
viaSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny,
    int rop, unsigned planemask, int trans_color)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    ViaCommandBuffer *cb = &pVia->cb;

    tdc->cmd = VIA_GEC_BLT | VIAACCELPATTERNROP(rop);
    tdc->patternAddr = (patternx * pVia->Bpp + patterny * pVia->Bpl);
    viaAccelTransparentHelper(tdc, cb, (trans_color != -1) ? 0x4000 : 0x0000,
	trans_color, FALSE);
}

static void
viaSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patOffx,
    int patOffy, int x, int y, int w, int h)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 patAddr;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    if (!w || !h)
	return;

    patAddr = (tdc->patternAddr >> 3) |
	((patOffy & 0x7) << 29) | ((patOffx & 0x7) << 26);
    sub = viaAccelClippingHelper(cb, y, tdc);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING_AGP(cb, 14);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), tdc->mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), dstBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	((pVia->Bpl >> 3) << 16));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), ((y - sub) << 16) | x);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION),
	(((h - 1) << 16) | (w - 1)));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PATADDR), patAddr);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), tdc->cmd);
    cb->flushFunc(cb);
}

/*
 * CPU to screen functions cannot use AGP due to complicated syncing. Therefore the
 * command buffer is flushed before new command emissions and viaFluchPCI is called
 * explicitly instead of cb->flushFunc() at the end of each CPU to screen function.
 * Should the buffer get completely filled again by a CPU to screen command emission,
 * a horrible error will occur.
 */

static void
viaSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg, int bg,
    int rop, unsigned planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int cmd;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | VIA_GEC_SRC_MONO |
	VIAACCELCOPYROP(rop);

    if (bg == -1) {
	cmd |= VIA_GEC_MSRC_TRANS;
    }

    tdc->cmd = cmd;
    tdc->fgColor = fg;
    tdc->bgColor = bg;

    cb->flushFunc(cb);

    viaAccelTransparentHelper(tdc, cb, 0x0, 0x0, FALSE);
}

static void
viaSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
    int y, int w, int h, int skipleft)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    if (skipleft) {
	viaSetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1),
	    (y + h - 1));
    }

    sub = viaAccelClippingHelper(cb, y, tdc);
    BEGIN_RING_AGP(cb, 4);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_BGCOLOR), tdc->bgColor);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_FGCOLOR), tdc->fgColor);
    viaAccelCopyHelper(cb, 0, 0, x, y - sub, w, h, 0,
	pScrn->fbOffset + sub * pVia->Bpl, tdc->mode, pVia->Bpl, pVia->Bpl,
	tdc->cmd);

    viaFlushPCI(cb);
    viaDisableClipping(pScrn);
}

static void
viaSetupForImageWrite(ScrnInfoPtr pScrn, int rop, unsigned planemask,
    int trans_color, int bpp, int depth)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_SRC_SYS | VIAACCELCOPYROP(rop);
    cb->flushFunc(cb);
    viaAccelTransparentHelper(tdc, cb, (trans_color != -1) ? 0x4000 : 0x0000,
	trans_color, FALSE);
}

static void
viaSubsequentImageWriteRect(ScrnInfoPtr pScrn, int x, int y, int w, int h,
    int skipleft)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    int sub;

    if (skipleft) {
	viaSetClippingRectangle(pScrn, (x + skipleft), y, (x + w - 1),
	    (y + h - 1));
    }

    sub = viaAccelClippingHelper(cb, y, tdc);
    viaAccelCopyHelper(cb, 0, 0, x, y - sub, w, h, 0,
	pScrn->fbOffset + pVia->Bpl * sub, tdc->mode, pVia->Bpl, pVia->Bpl,
	tdc->cmd);

    viaFlushPCI(cb);
    viaDisableClipping(pScrn);
}

static void
viaSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
    unsigned int planemask)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    viaAccelTransparentHelper(tdc, cb, 0x00, 0x00, FALSE);
    tdc->cmd = VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);
    tdc->fgColor = color;

    BEGIN_RING_AGP(cb, 6);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), tdc->mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_MONOPAT0), 0xFF);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_FGCOLOR), tdc->fgColor);
}

static void
viaSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1,
    int x2, int y2, int flags)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int dx, dy, cmd, tmp, error = 1;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    cmd = tdc->cmd | VIA_GEC_LINE;

    dx = x2 - x1;
    if (dx < 0) {
	dx = -dx;
	cmd |= VIA_GEC_DECX;	       /* line will be drawn from right */
	error = 0;
    }

    dy = y2 - y1;
    if (dy < 0) {
	dy = -dy;
	cmd |= VIA_GEC_DECY;	       /* line will be drawn from bottom */
    }

    if (dy > dx) {
	tmp = dy;
	dy = dx;
	dx = tmp;		       /* Swap 'dx' and 'dy' */
	cmd |= VIA_GEC_Y_MAJOR;	       /* Y major line */
    }

    if (flags & OMIT_LAST) {
	cmd |= VIA_GEC_LASTPIXEL_OFF;
    }

    sub = viaAccelClippingHelper(cb, (y1 < y2) ? y1 : y2, tdc);

    dstBase = pScrn->fbOffset + sub * pVia->Bpl;
    y1 -= sub;
    y2 -= sub;

    BEGIN_RING_AGP(cb, 14);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), dstBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	((pVia->Bpl >> 3) << 16));

    /*
     * major = 2*dmaj, minor = 2*dmin, err = -dmaj - ((bias >> octant) & 1) 
     * K1 = 2*dmin K2 = 2*(dmin - dmax) 
     * Error Term = (StartX<EndX) ? (2*dmin - dmax - 1) : (2*(dmin - dmax)) 
     */

    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_LINE_K1K2),
	((((dy << 1) & 0x3fff) << 16) | (((dy - dx) << 1) & 0x3fff)));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_LINE_XY), ((y1 << 16) | x1));
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION), dx);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_LINE_ERROR),
	(((dy << 1) - dx - error) & 0x3fff) | 0xFF0000);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), cmd);
    cb->flushFunc(cb);

}

static void
viaSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len,
    int dir)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstBase;
    int sub;

    sub = viaAccelClippingHelper(cb, y, tdc);
    dstBase = pScrn->fbOffset + sub * pVia->Bpl;

    BEGIN_RING_AGP(cb, 10);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTBASE), dstBase >> 3);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
	((pVia->Bpl >> 3) << 16));

    if (dir == DEGREES_0) {
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), ((y - sub) << 16) | x);
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION), (len - 1));
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), tdc->cmd | VIA_GEC_BLT);
    } else {
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DSTPOS), ((y - sub) << 16) | x);
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_DIMENSION), ((len - 1) << 16));
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GECMD), tdc->cmd | VIA_GEC_BLT);
    }
    cb->flushFunc(cb);
}

static void
viaSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
    unsigned int planemask, int length, unsigned char *pattern)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int cmd;
    CARD32 pat = *(CARD32 *) pattern;
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    viaAccelTransparentHelper(tdc, cb, 0x00, 0x00, FALSE);
    cmd = VIA_GEC_LINE | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(rop);

    if (bg == -1) {
	cmd |= VIA_GEC_MPAT_TRANS;
    }

    tdc->cmd = cmd;
    tdc->fgColor = fg;
    tdc->bgColor = bg;

    switch (length) {
    case 2:
	pat |= pat << 2;	       /* fall through */
    case 4:
	pat |= pat << 4;	       /* fall through */
    case 8:
	pat |= pat << 8;	       /* fall through */
    case 16:
	pat |= pat << 16;
    }

    tdc->pattern0 = pat;

    BEGIN_RING_AGP(cb, 8);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_GEMODE), tdc->mode);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_FGCOLOR), tdc->fgColor);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_BGCOLOR), tdc->bgColor);
    OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_MONOPAT0), tdc->pattern0);
}

static void
viaSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2,
    int y2, int flags, int phase)
{
    viaSubsequentSolidTwoPointLine(pScrn, x1, y1, x2, y2, flags);
}

static int
viaInitXAA(ScreenPtr pScreen)
{

    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    XAAInfoRecPtr xaaptr;

    /* 
     * General acceleration flags 
     */

    if (!(xaaptr = pVia->AccelInfoRec = XAACreateInfoRec()))
	return FALSE;

    xaaptr->Flags = PIXMAP_CACHE |
	OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER | MICROSOFT_ZERO_LINE_BIAS | 0;

    if (pScrn->bitsPerPixel == 8)
	xaaptr->CachePixelGranularity = 128;

    xaaptr->SetClippingRectangle = viaSetClippingRectangle;
    xaaptr->DisableClipping = viaDisableClipping;
    xaaptr->ClippingFlags = HARDWARE_CLIP_SOLID_FILL |
	HARDWARE_CLIP_SOLID_LINE |
	HARDWARE_CLIP_DASHED_LINE |
	HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
	HARDWARE_CLIP_MONO_8x8_FILL |
	HARDWARE_CLIP_COLOR_8x8_FILL |
	HARDWARE_CLIP_SCREEN_TO_SCREEN_COLOR_EXPAND | 0;

    xaaptr->Sync = viaAccelSync;

    xaaptr->SetupForScreenToScreenCopy = viaSetupForScreenToScreenCopy;
    xaaptr->SubsequentScreenToScreenCopy = viaSubsequentScreenToScreenCopy;
    xaaptr->ScreenToScreenCopyFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    xaaptr->SetupForSolidFill = viaSetupForSolidFill;
    xaaptr->SubsequentSolidFillRect = viaSubsequentSolidFillRect;
    xaaptr->SolidFillFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    xaaptr->SetupForMono8x8PatternFill = viaSetupForMono8x8PatternFill;
    xaaptr->SubsequentMono8x8PatternFillRect =
	viaSubsequentMono8x8PatternFillRect;
    xaaptr->Mono8x8PatternFillFlags = NO_PLANEMASK |
	HARDWARE_PATTERN_PROGRAMMED_BITS |
	HARDWARE_PATTERN_PROGRAMMED_ORIGIN | BIT_ORDER_IN_BYTE_MSBFIRST | 0;

    xaaptr->SetupForColor8x8PatternFill = viaSetupForColor8x8PatternFill;
    xaaptr->SubsequentColor8x8PatternFillRect =
	viaSubsequentColor8x8PatternFillRect;
    xaaptr->Color8x8PatternFillFlags = NO_PLANEMASK |
	NO_TRANSPARENCY |
	HARDWARE_PATTERN_PROGRAMMED_BITS |
	HARDWARE_PATTERN_PROGRAMMED_ORIGIN | 0;

    xaaptr->SetupForSolidLine = viaSetupForSolidLine;
    xaaptr->SubsequentSolidTwoPointLine = viaSubsequentSolidTwoPointLine;
    xaaptr->SubsequentSolidHorVertLine = viaSubsequentSolidHorVertLine;
    xaaptr->SolidBresenhamLineErrorTermBits = 14;
    xaaptr->SolidLineFlags = NO_PLANEMASK | ROP_NEEDS_SOURCE;

    xaaptr->SetupForDashedLine = viaSetupForDashedLine;
    xaaptr->SubsequentDashedTwoPointLine = viaSubsequentDashedTwoPointLine;
    xaaptr->DashPatternMaxLength = 8;
    xaaptr->DashedLineFlags = NO_PLANEMASK |
	ROP_NEEDS_SOURCE |
	LINE_PATTERN_POWER_OF_2_ONLY | LINE_PATTERN_MSBFIRST_LSBJUSTIFIED | 0;

    xaaptr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
	CPU_TRANSFER_PAD_DWORD |
	SCANLINE_PAD_DWORD |
	BIT_ORDER_IN_BYTE_MSBFIRST |
	LEFT_EDGE_CLIPPING | ROP_NEEDS_SOURCE | 0;

    xaaptr->SetupForScanlineCPUToScreenColorExpandFill =
	viaSetupForCPUToScreenColorExpandFill;
    xaaptr->SubsequentScanlineCPUToScreenColorExpandFill =
	viaSubsequentScanlineCPUToScreenColorExpandFill;
    xaaptr->ColorExpandBase = pVia->BltBase;
    xaaptr->ColorExpandRange = VIA_MMIO_BLTSIZE;

    xaaptr->ImageWriteFlags = NO_PLANEMASK |
	CPU_TRANSFER_PAD_DWORD |
	SCANLINE_PAD_DWORD |
	BIT_ORDER_IN_BYTE_MSBFIRST |
	LEFT_EDGE_CLIPPING | ROP_NEEDS_SOURCE | SYNC_AFTER_IMAGE_WRITE | 0;

    /*
     * Most Unichromes are much faster using processor to
     * framebuffer writes than using the 2D engine for this.
     * test with x11perf -shmput500!
     */

    if (pVia->Chipset != VIA_K8M800)
	xaaptr->ImageWriteFlags |= NO_GXCOPY;

    xaaptr->SetupForImageWrite = viaSetupForImageWrite;
    xaaptr->SubsequentImageWriteRect = viaSubsequentImageWriteRect;
    xaaptr->ImageWriteBase = pVia->BltBase;
    xaaptr->ImageWriteRange = VIA_MMIO_BLTSIZE;

    return XAAInit(pScreen, xaaptr);

}

/*
 * Mark Sync using the 2D blitter for AGP. NoOp for PCI.
 * In the future one could even launch a NULL PCI DMA command
 * to have an interrupt generated, provided it is possible to
 * write to the PCI DMA engines from the AGP command stream.
 */

static int
viaAccelMarkSync(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;

    ++pVia->curMarker;

    /*
     * Wrap around without possibly affecting the int sign bit. 
     */

    pVia->curMarker &= 0x7FFFFFFF;

    if (pVia->agpDMA) {
	BEGIN_RING_AGP(cb, 2);
	OUT_RING_QW_AGP(cb, H1_ADDR(VIA_REG_KEYCONTROL), 0x00);
	viaAccelSolidHelper(cb, 0, 0, 1, 1, pVia->markerOffset,
	    VIA_GEM_32bpp, 4, pVia->curMarker,
	    (0xF0 << 24) | VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT);
	cb->flushFunc(cb);
    }
    return pVia->curMarker;
}

/*
 * Wait for the value to get blitted, or in the PCI case for engine idle.
 */

static void
viaAccelWaitMarker(ScreenPtr pScreen, int marker)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 uMarker = marker;

    if (pVia->agpDMA) {
	while ((pVia->lastMarkerRead - uMarker) > (1 << 24))
	    pVia->lastMarkerRead = *pVia->markerBuf;
    } else {
	viaAccelSync(pScrn);
    }
}

#ifdef VIA_HAVE_EXA

/*
 * Exa functions. It is assumed that EXA does not exceed the blitter limits.
 */

static Bool
viaExaPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planeMask, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    if (exaGetPixmapPitch(pPixmap) & 7)
	return FALSE;

    if (!viaAccelSetMode(pPixmap->drawable.depth, tdc))
	return FALSE;

    if (!viaAccelPlaneMaskHelper(tdc, planeMask))
	return FALSE;
    viaAccelTransparentHelper(tdc, cb, 0x0, 0x0, TRUE);

    tdc->cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | VIAACCELPATTERNROP(alu);

    tdc->fgColor = fg;

    return TRUE;
}

static void
viaExaSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 dstPitch, dstOffset;

    int w = x2 - x1, h = y2 - y1;

    dstPitch = exaGetPixmapPitch(pPixmap);
    dstOffset = exaGetPixmapOffset(pPixmap);

    viaAccelSolidHelper(cb, x1, y1, w, h, dstOffset,
	tdc->mode, dstPitch, tdc->fgColor, tdc->cmd);
    cb->flushFunc(cb);
}

static void
viaExaDoneSolidCopy(PixmapPtr pPixmap)
{
}

static Bool
viaExaPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir,
    int ydir, int alu, Pixel planeMask)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;

    if (pSrcPixmap->drawable.bitsPerPixel !=
	pDstPixmap->drawable.bitsPerPixel)
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

    if (!viaAccelPlaneMaskHelper(tdc, planeMask))
	return FALSE;
    viaAccelTransparentHelper(tdc, cb, 0x0, 0x0, TRUE);

    return TRUE;
}

static void
viaExaCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY,
    int width, int height)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ViaCommandBuffer *cb = &pVia->cb;
    ViaTwodContext *tdc = &pVia->td;
    CARD32 srcOffset = tdc->srcOffset;
    CARD32 dstOffset = exaGetPixmapOffset(pDstPixmap);

    if (!width || !height)
	return;

    viaAccelCopyHelper(cb, srcX, srcY, dstX, dstY, width, height,
	srcOffset, dstOffset, tdc->mode, tdc->srcPitch,
	exaGetPixmapPitch(pDstPixmap), tdc->cmd);
    cb->flushFunc(cb);
}

#ifdef XF86DRI

/*
 * Use PCI DMA if we can. Really, if the system alignments don't match it is worth doing 
 * an extra copy to avoid reading from the frame-buffer which is painfully slow.
 */

static Bool
viaExaDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
    char *dst, int dst_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    drm_via_dmablit_t blit;
    unsigned srcPitch = exaGetPixmapPitch(pSrc);
    unsigned bytesPerPixel = pSrc->drawable.bitsPerPixel >> 3;
    unsigned srcOffset =
	exaGetPixmapOffset(pSrc) + y * srcPitch + x * bytesPerPixel;
    int err;
    char *bounce = NULL;
    char *bounceAligned = NULL;
    unsigned bouncePitch = 0;

    if (!pVia->directRenderingEnabled)
	return FALSE;

    if ((srcPitch & 3) || (srcOffset & 3)) {
	ErrorF("VIA EXA download src_pitch misaligned\n");
	return FALSE;
    }

    blit.num_lines = h;
    blit.line_length = w * bytesPerPixel;
    blit.fb_addr = srcOffset;
    blit.fb_stride = srcPitch;
    blit.mem_addr = dst;
    blit.mem_stride = dst_pitch;
    blit.bounce_buffer = 0;
    blit.to_fb = 0;

    if (((unsigned long)dst & 15) || (dst_pitch & 15)) {
	bouncePitch = (w * bytesPerPixel + 15) & ~15;
	bounce = (char *)xalloc(bouncePitch * h + 15);
	if (!bounce)
	    return FALSE;
	bounceAligned = (char *)(((unsigned long)bounce + 15) & ~15);
	blit.mem_addr = bounceAligned;
	blit.mem_stride = bouncePitch;
    }

    while (-EAGAIN == (err =
	    drmCommandWriteRead(pVia->drmFD, DRM_VIA_DMA_BLIT, &blit,
		sizeof(blit)))) ;
    if (!err) {
	while (-EAGAIN == (err =
		drmCommandWrite(pVia->drmFD, DRM_VIA_BLIT_SYNC, &blit.sync,
		    sizeof(blit.sync)))) ;
    }
    if (!err && bounce) {
	unsigned i;

	for (i = 0; i < h; ++i) {
	    memcpy(dst, bounceAligned, blit.line_length);
	    dst += dst_pitch;
	    bounceAligned += bouncePitch;
	}
    }
    if (bounce)
	xfree(bounce);
    return (err == 0);
}

/*
 * I'm not sure upload is necessary. Seems buggy for widths below 65, and I'd guess that in
 * most situations, CPU direct writes are faster. Use DMA only when alignments match. At least
 * it saves some CPU cycles.
 */

static Bool
viaExaUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h, char *src,
    int src_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    drm_via_dmablit_t blit;
    unsigned dstPitch = exaGetPixmapPitch(pDst);
    unsigned bytesPerPixel = pDst->drawable.bitsPerPixel >> 3;
    unsigned dstOffset =
	exaGetPixmapOffset(pDst) + y * dstPitch + x * bytesPerPixel;
    int err;

    if (!pVia->directRenderingEnabled)
	return FALSE;

    if (((unsigned long)src & 15) || (src_pitch & 15))
	return FALSE;

    if ((dstPitch & 3) || (dstOffset & 3))
	return FALSE;

    blit.line_length = w * bytesPerPixel;
    if (blit.line_length < 65)
	return FALSE;

    blit.num_lines = h;
    blit.fb_addr = dstOffset;
    blit.fb_stride = dstPitch;
    blit.mem_addr = src;
    blit.mem_stride = src_pitch;
    blit.bounce_buffer = 0;
    blit.to_fb = 1;

    while (-EAGAIN == (err =
	    drmCommandWriteRead(pVia->drmFD, DRM_VIA_DMA_BLIT, &blit,
		sizeof(blit)))) ;
    if (err < 0)
	return FALSE;

    while (-EAGAIN == (err = drmCommandWrite(pVia->drmFD, DRM_VIA_BLIT_SYNC,
		&blit.sync, sizeof(blit.sync)))) ;
    return (err == 0);
}
#endif

/*
 * Init EXA. Alignments are 2D engine constraints.
 */

static ExaDriverPtr
viaInitExa(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    ExaDriverPtr pExa = (ExaDriverPtr) xnfcalloc(sizeof(ExaDriverRec), 1);

    if (!pExa)
	return NULL;

    pExa->card.memoryBase = pVia->FBBase;
    pExa->card.memorySize = pVia->FBFreeEnd;
    pExa->card.offScreenBase = pScrn->virtualY * pVia->Bpl;
    pExa->card.pixmapOffsetAlign = 32;
    pExa->card.pixmapPitchAlign = 16;
    pExa->card.flags = EXA_OFFSCREEN_PIXMAPS;
    pExa->card.maxX = 2047;
    pExa->card.maxY = 2047;

    pExa->accel.WaitMarker = viaAccelWaitMarker;
    pExa->accel.MarkSync = viaAccelMarkSync;
    pExa->accel.PrepareSolid = viaExaPrepareSolid;
    pExa->accel.Solid = viaExaSolid;
    pExa->accel.DoneSolid = viaExaDoneSolidCopy;
    pExa->accel.PrepareCopy = viaExaPrepareCopy;
    pExa->accel.Copy = viaExaCopy;
    pExa->accel.DoneCopy = viaExaDoneSolidCopy;

#if defined(XF86DRI) && defined(linux)
    if (pVia->directRenderingEnabled) {
	if ((pVia->drmVerMajor > 2) ||
	    ((pVia->drmVerMajor == 2) && (pVia->drmVerMinor >= 7))) {
	    pExa->accel.UploadToScreen = viaExaUploadToScreen;
	    pExa->accel.DownloadFromScreen = viaExaDownloadFromScreen;
	}
    }
#endif

    /*
     * Composite not supported. Could we use the 3D engine?
     */

    if (!exaDriverInit(pScreen, pExa)) {
	xfree(pExa);
	return NULL;
    }

    return pExa;
}

#endif /* VIA_HAVE_EXA */

/*
 * Acceleration init function. Sets up offscreen memory disposition, initializes engines
 * and acceleration method.
 */

Bool
viaInitAccel(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    BoxRec AvailFBArea;
    int maxY;

    pVia->VQStart = 0;
    if (((pVia->FBFreeEnd - pVia->FBFreeStart) >= VIA_VQ_SIZE) &&
	pVia->VQEnable) {
	pVia->VQStart = pVia->FBFreeEnd - VIA_VQ_SIZE;
	pVia->VQEnd = pVia->VQStart + VIA_VQ_SIZE - 1;
	pVia->FBFreeEnd -= VIA_VQ_SIZE;
    }

    viaInitialize2DEngine(pScrn);

    if (pVia->hwcursor) {
	pVia->FBFreeEnd -= VIA_CURSOR_SIZE;
	pVia->CursorStart = pVia->FBFreeEnd;
    }

    if (Success != viaSetupCBuffer(pScrn, &pVia->cb, 0)) {
	pVia->NoAccel = TRUE;
	return FALSE;
    }

    /*
     * Sync marker space.
     */

    pVia->FBFreeEnd -= 32;
    pVia->markerOffset = (pVia->FBFreeEnd + 31) & ~31;
    pVia->markerBuf = (CARD32 *) ((char *)pVia->FBBase + pVia->markerOffset);

#ifdef VIA_HAVE_EXA

    if (pVia->useEXA) {
	pVia->exaDriverPtr = viaInitExa(pScreen);
	if (!pVia->exaDriverPtr) {

	    /*
	     * Docs recommend turning off also Xv here, but we handle this
	     * case with the old linear offscreen FB manager through
	     * VIAInitLinear.
	     */

	    pVia->NoAccel = TRUE;
	    return FALSE;
	}

	pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart) / 2;

	return TRUE;
    }
#endif

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;

    /*
     * Memory distribution for XAA is tricky. We'd like to make the 
     * pixmap cache no larger than 3 x visible screen size, otherwise
     * XAA may get slow for some reason. 
     */

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
	pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart) / 2;
	maxY = pScrn->virtualY + (pVia->driSize / pVia->Bpl);
    } else
#endif
    {
	maxY = pVia->FBFreeEnd / pVia->Bpl;
    }
    if (maxY > 4 * pScrn->virtualY)
	maxY = 4 * pScrn->virtualY;

    pVia->FBFreeStart = (maxY + 1) * pVia->Bpl;

    AvailFBArea.y2 = maxY;
    xf86InitFBManager(pScreen, &AvailFBArea);
    VIAInitLinear(pScreen);

    pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart - pVia->Bpl);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	    "Using %d lines for offscreen memory.\n",
	    AvailFBArea.y2 - pScrn->virtualY));

    return viaInitXAA(pScreen);
}

/*
 * Free used acceleration resources.
 */

void
viaExitAccel(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    viaAccelSync(pScrn);

#ifdef VIA_HAVE_EXA
    if (pVia->useEXA) {
	if (pVia->exaDriverPtr) {
	    exaDriverFini(pScreen);
	}
	xfree(pVia->exaDriverPtr);
	pVia->exaDriverPtr = NULL;
	viaTearDownCBuffer(&pVia->cb);
	return;
    }
#endif
    if (pVia->AccelInfoRec) {
	XAADestroyInfoRec(pVia->AccelInfoRec);
	pVia->AccelInfoRec = NULL;
	viaTearDownCBuffer(&pVia->cb);
    }
}

/*
 * DGA accelerated functions go here and let them be independent of acceleration 
 * method.
 */

void
viaDGABlitRect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h,
    int dstx, int dsty)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    ViaCommandBuffer *cb = &pVia->cb;
    unsigned dstOffset = pScrn->fbOffset + dsty * pVia->Bpl;
    unsigned srcOffset = pScrn->fbOffset + srcy * pVia->Bpl;

    if (!w || !h)
	return;

    if (!pVia->NoAccel) {

	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;
	CARD32 cmd = VIA_GEC_BLT | VIAACCELCOPYROP(GXcopy);

	if (xdir < 0)
	    cmd |= VIA_GEC_DECX;
	if (ydir < 0)
	    cmd |= VIA_GEC_DECY;

	viaAccelSetMode(pScrn->bitsPerPixel, tdc);
	viaAccelTransparentHelper(tdc, cb, 0x0, 0x0, FALSE);
	viaAccelCopyHelper(cb, srcx, 0, dstx, 0, w, h, srcOffset, dstOffset,
	    tdc->mode, pVia->Bpl, pVia->Bpl, cmd);
	pVia->dgaMarker = viaAccelMarkSync(pScrn->pScreen);
	cb->flushFunc(cb);
    }
}

void
viaDGAFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h,
    unsigned long color)
{
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned dstBase = pScrn->fbOffset + y * pVia->Bpl;
    ViaTwodContext *tdc = &pVia->td;
    ViaCommandBuffer *cb = &pVia->cb;
    CARD32 cmd = VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT |
	VIAACCELPATTERNROP(GXcopy);

    if (!w || !h)
	return;

    if (!pVia->NoAccel) {
	viaAccelSetMode(pScrn->bitsPerPixel, tdc);
	viaAccelTransparentHelper(tdc, cb, 0x0, 0x0, FALSE);
	viaAccelSolidHelper(cb, x, 0, w, h, dstBase, tdc->mode,
	    pVia->Bpl, color, cmd);
	pVia->dgaMarker = viaAccelMarkSync(pScrn->pScreen);
	cb->flushFunc(cb);
    }
}

void
viaDGAWaitMarker(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    viaAccelWaitMarker(pScrn->pScreen, pVia->dgaMarker);
}
