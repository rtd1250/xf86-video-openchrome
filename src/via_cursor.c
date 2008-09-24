/*
 * Copyright 2007 The Openchrome Project [openchrome.org]
 * Copyright 1998-2007 VIA Technologies, Inc. All Rights Reserved.
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
 */

/*************************************************************************
 *
 *  File:       via_cursor.c
 *  Content:    Hardware cursor support for VIA/S3G UniChrome
 *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "via.h"
#include "via_driver.h"
#include "via_regs.h"
#include "via_id.h"

#ifdef ARGB_CURSOR
#include "cursorstr.h"
#endif

static void viaCursorLoadImage(ScrnInfoPtr pScrn, unsigned char *src);
static void viaCursorSetPosition(ScrnInfoPtr pScrn, int x, int y);
static void viaCursorSetColors(ScrnInfoPtr pScrn, int bg, int fg);
static Bool viaCursorHWUse(ScreenPtr screen, CursorPtr cursor);
static void viaCursorHWHide(ScrnInfoPtr pScrn);

#ifdef ARGB_CURSOR
static void viaCursorARGBShow(ScrnInfoPtr pScrn);
static void viaCursorARGBHide(ScrnInfoPtr pScrn);
static void viaCursorARGBSetPosition(ScrnInfoPtr pScrn, int x, int y);
static Bool viaCursorARGBUse(ScreenPtr pScreen, CursorPtr pCurs);
static void viaCursorARGBLoad(ScrnInfoPtr pScrn, CursorPtr pCurs);
#endif

#ifdef ARGB_CURSOR
static void
viaCursorARGBInit(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "viaCursorARGBInit\n"));

    unsigned long fbOffset = pScrn->fbOffset + pVia->cursor->fbCursorStart;

    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
            if (pBIOSInfo->FirstCRTC->IsActive) {
                VIASETREG(VIA_REG_PRIM_HI_FBOFFSET, fbOffset);
                /* Set 0 as transparent color key. */
                VIASETREG(VIA_REG_PRIM_HI_TRANSCOLOR, 0);
                VIASETREG(VIA_REG_PRIM_HI_FIFO, 0x0D000D0F);
                VIASETREG(VIA_REG_PRIM_HI_INVTCOLOR, 0X00FFFFFF);
                VIASETREG(VIA_REG_V327_HI_INVTCOLOR, 0X00FFFFFF);
            }
            if (pBIOSInfo->SecondCRTC->IsActive) {
                VIASETREG(VIA_REG_HI_FBOFFSET, fbOffset);
                /* Set 0 as transparent color key. */
                VIASETREG(VIA_REG_HI_TRANSPARENT_COLOR, 0);
                VIASETREG(VIA_REG_HI_INVTCOLOR, 0X00FFFFFF);
                VIASETREG(ALPHA_V3_PREFIFO_CONTROL, 0xE0000);
                VIASETREG(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);
            }
            break;
        default:
            VIASETREG(VIA_REG_HI_FBOFFSET, fbOffset);
            VIASETREG(VIA_REG_HI_TRANSPARENT_COLOR, 0);
            VIASETREG(VIA_REG_HI_INVTCOLOR, 0X00FFFFFF);
            VIASETREG(ALPHA_V3_PREFIFO_CONTROL, 0xE0000);
            VIASETREG(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);
            break;
    }
}
#endif

void
viaCursorSetFB(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (!pVia->cursor->fbCursorStart
        && ((pVia->FBFreeEnd - pVia->FBFreeStart) > pVia->cursor->size)) {
        pVia->cursor->fbCursorStart = pVia->FBFreeEnd - pVia->cursor->size;
        pVia->FBFreeEnd -= pVia->cursor->size;
    }
}

Bool
viaCursorHWInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIACursorHWInit\n"));

    infoPtr = xf86CreateCursorInfoRec();
    if (!infoPtr)
        return FALSE;

    pVia->cursor->info = infoPtr;

    infoPtr->MaxWidth = pVia->cursor->maxWidth;
    infoPtr->MaxHeight = pVia->cursor->maxHeight;
    infoPtr->Flags = (HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                      /*HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK | */
                      HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                      HARDWARE_CURSOR_INVERT_MASK |
                      HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
                      0);

    if (pVia->cursor->maxWidth == 64)
        infoPtr->Flags |= HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64;
    else if (pVia->cursor->maxWidth == 32)
        infoPtr->Flags |= HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32;
    else {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "VIACursorHWInit: unhandled width\n");
        return FALSE;
    }
/*
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        infoPtr->Flags |= HARDWARE_CURSOR_ARGB;
*/
    infoPtr->SetCursorColors = viaCursorSetColors;
    infoPtr->SetCursorPosition = viaCursorSetPosition;
    infoPtr->LoadCursorImage = viaCursorLoadImage;
    infoPtr->HideCursor = viaCursorHide;
    infoPtr->ShowCursor = viaCursorShow;
    infoPtr->UseHWCursor = viaCursorHWUse;

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled) {
        infoPtr->UseHWCursorARGB = viaCursorARGBUse;
        infoPtr->LoadCursorARGB = viaCursorARGBLoad;
    }
#endif

    viaCursorSetFB(pScrn);

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        viaCursorARGBInit(pScrn);
#endif

    /* Set cursor location in frame buffer. */
    VIASETREG(VIA_REG_CURSOR_MODE, pVia->cursor->fbCursorStart);
    viaCursorHWHide(pScrn);

    return xf86InitCursor(pScreen, infoPtr);
}

static void
viaCursorHWShow(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 mode;

    mode = VIAGETREG(VIA_REG_CURSOR_MODE);
    mode &= ~0x80000003;

    /* Hardware cursor size */
    if (pVia->cursor->maxWidth == 32)
        mode |= 0x00000002 ;
    
    /* Enable cursor */
    mode |= 0x00000001 ;
    
    if (pVia->pBIOSInfo->SecondCRTC->IsActive)
        mode |= 0x80000000 ;
    
    /* Turn on hardware cursor. */
    VIASETREG(VIA_REG_CURSOR_MODE, mode);
    
}

static void
viaCursorHWHide(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 mode = VIAGETREG(VIA_REG_CURSOR_MODE);

    /* Turn hardware cursor off. */
    VIASETREG(VIA_REG_CURSOR_MODE, mode & 0xFFFFFFFE);
}


void
viaCursorShow(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        viaCursorARGBShow(pScrn);
    else
#endif
        viaCursorHWShow(pScrn);
}

void
viaCursorHide(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        viaCursorARGBHide(pScrn);
    else
#endif
        viaCursorHWHide(pScrn);
}

static void
viaCursorLoadImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int x, y, i;

    viaAccelSync(pScrn);
    CARD32 *dst = (CARD32 *) (pVia->FBBase + pVia->cursor->fbCursorStart);

    memset(dst, 0x00, pVia->cursor->size);
#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled) {
        viaCursorARGBHide(pScrn);
        /* Convert monochrome to ARGB. */
        int width = pVia->cursor->maxWidth / 8;

        for (y = 0; y < (pVia->cursor->maxHeight / 8) * 2; y++) {
            for (x = 0; x < width; x++) {
                char t = *(src + width); /* is transparent? */
                char fb = *src++; /* foreground or background ? */

                for (i = 7; i >= 0; i--) {
                    if (t & (1 << i))
                        *dst++ = 0x00000000; /* transparent */
                    else
                        *dst++ = fb & (1 << i) ?
                                0xFF000000 | pVia->cursor->foreground :
                                0xFF000000 | pVia->cursor->background;
                }
            }
            src += width;
        }
    } else
#endif
    {
        viaCursorHWHide(pScrn);
        /* Upload the cursor image to the frame buffer. */
        memcpy(dst, src, pVia->cursor->size);
    }
    viaCursorShow(pScrn);
}

static void
viaCursorHWSetPosition(ScrnInfoPtr pScrn, int x, int y, int xoff, int yoff)
{
    VIAPtr pVia = VIAPTR(pScrn);

    /* viaCursorHWHide(pScrn); */
    VIASETREG(VIA_REG_CURSOR_ORG, ((xoff << 16) | (yoff & 0x003f)));
    VIASETREG(VIA_REG_CURSOR_POS, ((x << 16) | (y & 0x07ff)));
    /* viaCursorHWShow(pScrn); */
}

static void
viaCursorSetPosition(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    unsigned char xoff, yoff;

    if (x < 0) {
        xoff = ((-x) & 0xFE);
        x = 0;
    } else {
        xoff = 0;
    }

    if (y < 0) {
        yoff = ((-y) & 0xFE);
        y = 0;
    } else {
        yoff = 0;
        /* LCD Expand Mode Cursor Y Position Re-Calculated */
        if (pBIOSInfo->scaleY) {
            y = (int)(((pBIOSInfo->panelY * y) + (pBIOSInfo->resY >> 1))
                      / pBIOSInfo->resY);
        }
    }

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        viaCursorARGBSetPosition(pScrn, x, y);
    else
#endif
        viaCursorHWSetPosition(pScrn, x, y, xoff, yoff);
}


static void
viaCursorSetColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    VIAPtr pVia = VIAPTR(pScrn);

    pVia->cursor->foreground = fg;
    pVia->cursor->background = bg;

#ifdef ARGB_CURSOR
    if (pVia->cursor->isARGBSupported && pVia->cursor->isARGBEnabled)
        return;
#endif

    VIASETREG(VIA_REG_CURSOR_FG, fg);
    VIASETREG(VIA_REG_CURSOR_BG, bg);
}

void
viaCursorStore(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "viaCursorStore\n"));

    if (pVia->cursor->image) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "viaCursorStore: stale image left.\n");
        xfree(pVia->cursor->image);
    }

    pVia->cursor->image = xcalloc(1, pVia->cursor->size);
    if (pVia->cursor->image)
        memcpy(pVia->cursor->image, pVia->FBBase + pVia->cursor->fbCursorStart,
               pVia->cursor->size);

    pVia->cursor->foreground = (CARD32) VIAGETREG(VIA_REG_CURSOR_FG);
    pVia->cursor->background = (CARD32) VIAGETREG(VIA_REG_CURSOR_BG);
    pVia->cursor->mode = (CARD32) VIAGETREG(VIA_REG_CURSOR_MODE);
}

void
viaCursorRestore(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "viaCursorRestore\n"));

    if (pVia->cursor->image) {
        memcpy(pVia->FBBase + pVia->cursor->fbCursorStart, pVia->cursor->image,
               pVia->cursor->size);
        VIASETREG(VIA_REG_CURSOR_FG, pVia->cursor->foreground);
        VIASETREG(VIA_REG_CURSOR_BG, pVia->cursor->background);
        VIASETREG(VIA_REG_CURSOR_MODE, pVia->cursor->mode);
        xfree(pVia->cursor->image);
        pVia->cursor->image = NULL;
    } else
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "viaCursorRestore: No cursor image stored.\n");
}

#ifdef ARGB_CURSOR

static void
viaCursorARGBShow(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
            /* Turn on hardware icon cursor. */
            if (pVia->pBIOSInfo->FirstCRTC->IsActive)
                VIASETREG(VIA_REG_PRIM_HI_CTRL, 0x76000005);
            if (pVia->pBIOSInfo->SecondCRTC->IsActive)
                VIASETREG(VIA_REG_HI_CONTROL, 0xf6000005);
            break;
        default:
            VIASETREG(VIA_REG_HI_CONTROL, 0xf6000005);
            break;
    }

}

static void
viaCursorARGBHide(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 hiControl;

    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
            if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                hiControl = VIAGETREG(VIA_REG_PRIM_HI_CTRL);
                /* Turn hardware icon cursor off. */
                VIASETREG(VIA_REG_PRIM_HI_CTRL, hiControl & 0xFFFFFFFE);
            }
            if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                hiControl = VIAGETREG(VIA_REG_HI_CONTROL);
                /* Turn hardware icon cursor off. */
                VIASETREG(VIA_REG_HI_CONTROL, hiControl & 0xFFFFFFFE);
            }
            break;
        default:
            hiControl = VIAGETREG(VIA_REG_HI_CONTROL);
            /* Turn hardware icon cursor off. */
            VIASETREG(VIA_REG_HI_CONTROL, hiControl & 0xFFFFFFFE);
    }
}

static void
viaCursorARGBSetPosition(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr pVia = VIAPTR(pScrn);

    /* viaCursorARGBHide(pScrn); */
    switch (pVia->Chipset) {
        case VIA_CX700:
        /* case VIA_CN750: */
        case VIA_P4M890:
        case VIA_P4M900:
            if (pVia->pBIOSInfo->FirstCRTC->IsActive) {
                /* Set hardware icon position. */
                VIASETREG(VIA_REG_PRIM_HI_POSSTART, ((x << 16) | (y & 0x07ff)));
                VIASETREG(VIA_REG_PRIM_HI_CENTEROFFSET,
                          ((0 << 16) | (0 & 0x07ff)));
            }
            if (pVia->pBIOSInfo->SecondCRTC->IsActive) {
                /* Set hardware icon position. */
                VIASETREG(VIA_REG_HI_POSSTART, ((x << 16) | (y & 0x07ff)));
                VIASETREG(VIA_REG_HI_CENTEROFFSET, ((0 << 16) | (0 & 0x07ff)));
            }
            break;
        default:
            /* Set hardware icon position. */
            VIASETREG(VIA_REG_HI_POSSTART, ((x << 16) | (y & 0x07ff)));
            VIASETREG(VIA_REG_HI_CENTEROFFSET, ((0 << 16) | (0 & 0x07ff)));
            break;
    }
    /* viaCursorARGBShow(pScrn); */
}

static Bool
viaCursorARGBUse(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    return (pVia->cursor->isHWCursorEnabled
            && pVia->cursor->isARGBSupported
            && pVia->cursor->isARGBEnabled
            && pCurs->bits->width <= pVia->cursor->maxWidth
            && pCurs->bits->height <= pVia->cursor->maxHeight);
}

static void
viaCursorARGBLoad(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    CARD32 *dst = NULL;
    CARD32 *image = pCurs->bits->argb;
    int x, y, w, h;

    dst = (CARD32 *) (pVia->FBBase + pVia->cursor->fbCursorStart);

    if (!image)
        return;

    w = pCurs->bits->width;
    if (w > pVia->cursor->maxWidth)
        w = pVia->cursor->maxWidth;

    h = pCurs->bits->height;
    if (h > pVia->cursor->maxHeight)
        h = pVia->cursor->maxHeight;

    memset(dst, 0, pVia->cursor->size);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++)
            *dst++ = *image++;

        /* Pad to the right with transparent. */
        for (; x < pVia->cursor->maxWidth; x++)
            *dst++ = 0;
    }

    /* Pad below with transparent. */
    for (; y < pVia->cursor->maxHeight; y++) {
        for (x = 0; x < pVia->cursor->maxWidth; x++)
            *dst++ = 0;
    }
    viaCursorShow(pScrn);
}

#endif

Bool
viaCursorRecInit(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (!pVia->cursor)
        pVia->cursor =
                (ViaCursorInfoPtr) xnfcalloc(sizeof(ViaCursorInfoRec), 1);

    if (pVia->cursor) {
        ViaCursorInfoPtr cursor = pVia->cursor;

        switch (pVia->Chipset) {
            case VIA_CLE266:
            case VIA_KM400:
            case VIA_K8M800:
                cursor->isARGBSupported = FALSE;
                cursor->isARGBEnabled = FALSE;
                cursor->maxWidth = 32;
                cursor->maxHeight = 32;
                cursor->size = ((cursor->maxWidth * cursor->maxHeight) / 8) * 2;
                break;
            default:
                cursor->isARGBSupported = TRUE;
                cursor->isARGBEnabled = TRUE;
                cursor->maxWidth = 64;
                cursor->maxHeight = 64;
                cursor->size = cursor->maxWidth * cursor->maxHeight * 4;
                break;
        }
    }

    return pVia->cursor != NULL;
}

void
viaCursorRecDestroy(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    if (pVia->cursor)
        xfree(pVia->cursor);
}

static Bool
viaCursorHWUse(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    return (pVia->cursor->isHWCursorEnabled
            /* Can't enable HW cursor on both CRTCs at the same time. */
            && !(pVia->pBIOSInfo->FirstCRTC->IsActive
                 && pVia->pBIOSInfo->SecondCRTC->IsActive)
            && pCurs->bits->width <= pVia->cursor->maxWidth
            && pCurs->bits->height <= pVia->cursor->maxHeight);
}
