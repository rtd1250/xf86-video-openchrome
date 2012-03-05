/*
 * Copyright 2011 The Openchrome Project  [openchrome.org]
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
#include "xf86Crtc.h"

#include "globals.h"
#include "via_driver.h"

static void
ViaMMIODisable(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    switch (pVia->Chipset) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            ViaSeqMask(hwp, 0x1A, 0x00, 0x08);
            break;
        default:
            ViaSeqMask(hwp, 0x1A, 0x00, 0x60);
            break;
    }
}

void
VIAUnmapMem(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAUnmapMem\n"));

    ViaMMIODisable(pScrn);

#ifdef XSERVER_LIBPCIACCESS
    if (pVia->MapBase)
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->MapBase,
                               VIA_MMIO_REGSIZE);

    if (pVia->BltBase)
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->BltBase,
                               VIA_MMIO_BLTSIZE);

    if (pVia->FBBase)
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->FBBase,
                               pVia->videoRambytes);
#else
    if (pVia->MapBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->MapBase,
                        VIA_MMIO_REGSIZE);

    if (pVia->BltBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->BltBase,
                        VIA_MMIO_BLTSIZE);

    if (pVia->FBBase)
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->FBBase,
                        pVia->videoRambytes);
#endif
}

static void
ViaMMIOEnable(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    switch (pVia->Chipset) {
        case VIA_K8M890:
        case VIA_CX700:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            ViaSeqMask(hwp, 0x1A, 0x08, 0x08);
            break;
        default:
            if (pVia->IsSecondary)
                ViaSeqMask(hwp, 0x1A, 0x38, 0x38);
            else
                ViaSeqMask(hwp, 0x1A, 0x68, 0x68);
            break;
    }
}

static Bool
VIAMapMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef XSERVER_LIBPCIACCESS
    pVia->MmioBase = pVia->PciInfo->regions[1].base_addr;
    int err;
#else
    pVia->MmioBase = pVia->PciInfo->memBase[1];
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapMMIO\n"));

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase, VIA_MMIO_REGSIZE);

#ifdef XSERVER_LIBPCIACCESS
    err = pci_device_map_range(pVia->PciInfo,
                               pVia->MmioBase,
                               VIA_MMIO_REGSIZE,
                               PCI_DEV_MAP_FLAG_WRITABLE,
                               (void **)&pVia->MapBase);

    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to map mmio BAR. %s (%d)\n", strerror(err), err);
        return FALSE;
    }
#else
    pVia->MapBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase, VIA_MMIO_REGSIZE);
    if (!pVia->MapBase)
        return FALSE;
#endif

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping BitBlt MMIO @ 0x%lx with size 0x%x\n",
               pVia->MmioBase + VIA_MMIO_BLTBASE, VIA_MMIO_BLTSIZE);

#ifdef XSERVER_LIBPCIACCESS
    err = pci_device_map_range(pVia->PciInfo,
                               pVia->MmioBase + VIA_MMIO_BLTBASE,
                               VIA_MMIO_BLTSIZE,
                               PCI_DEV_MAP_FLAG_WRITABLE,
                               (void **)&pVia->BltBase);

    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to map blt BAR. %s (%d)\n", strerror(err), err);
        return FALSE;
    }
#else
    pVia->BltBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                                  pVia->MmioBase + VIA_MMIO_BLTBASE,
                                  VIA_MMIO_BLTSIZE);
    if (!pVia->BltBase)
        return FALSE;
#endif

    if (!pVia->MapBase || !pVia->BltBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "BitBlit could not be mapped.\n");
        return FALSE;
    }

    /* Memory mapped IO for mpeg engine. */
    pVia->MpegMapBase = pVia->MapBase + 0xc00;

    /* Set up MMIO vgaHW. */
    {
        vgaHWPtr hwp = VGAHWPTR(pScrn);
        CARD8 val;

        vgaHWSetMmioFuncs(hwp, pVia->MapBase, 0x8000);

        val = hwp->readEnable(hwp);
        hwp->writeEnable(hwp, val | 0x01);

        val = hwp->readMiscOut(hwp);
        hwp->writeMiscOut(hwp, val | 0x01);

        /* Unlock extended IO space. */
        ViaSeqMask(hwp, 0x10, 0x01, 0x01);

        ViaMMIOEnable(pScrn);

        vgaHWSetMmioFuncs(hwp, pVia->MapBase, 0x8000);

        /* Unlock CRTC. */
        ViaCrtcMask(hwp, 0x47, 0x00, 0x01);

        vgaHWGetIOBase(hwp);
    }
    return TRUE;
}

static Bool
VIAMapFB(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

#ifdef XSERVER_LIBPCIACCESS
    if (pVia->Chipset == VIA_VX900) {
        pVia->FrameBufferBase = pVia->PciInfo->regions[2].base_addr;
    } else {
        pVia->FrameBufferBase = pVia->PciInfo->regions[0].base_addr;
    }
    int err;
#else
    if (pVia->Chipset == VIA_VX900) {
        pVia->FrameBufferBase = pVia->PciInfo->memBase[2];
    } else {
        pVia->FrameBufferBase = pVia->PciInfo->memBase[0];
    }
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAMapFB\n"));
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "mapping framebuffer @ 0x%lx with size 0x%lx\n",
               pVia->FrameBufferBase, pVia->videoRambytes);

    if (pVia->videoRambytes) {

#ifndef XSERVER_LIBPCIACCESS
         *
         * FIXME: This is a hack to get rid of offending wrongly sized
         * MTRR regions set up by the VIA BIOS. Should be taken care of
         * in the OS support layer.
         *

        unsigned char *tmp;

        tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                            pVia->FrameBufferBase, pVia->videoRambytes);
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) tmp, pVia->videoRambytes);

         *
         * And, as if this wasn't enough, 2.6 series kernels don't
         * remove MTRR regions on the first attempt. So try again.
         *

        tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                            pVia->FrameBufferBase, pVia->videoRambytes);
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) tmp, pVia->videoRambytes);

         *
         * End of hack.
         *
#endif

#ifdef XSERVER_LIBPCIACCESS
        err = pci_device_map_range(pVia->PciInfo, pVia->FrameBufferBase,
                                   pVia->videoRambytes,
                                   (PCI_DEV_MAP_FLAG_WRITABLE |
                                    PCI_DEV_MAP_FLAG_WRITE_COMBINE),
                                   (void **)&pVia->FBBase);
        if (err) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Unable to map mmio BAR. %s (%d)\n", strerror(err), err);
            return FALSE;
        }
#else
        pVia->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                                     pVia->PciTag, pVia->FrameBufferBase,
                                     pVia->videoRambytes);

        if (!pVia->FBBase) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Internal error: could not map framebuffer\n");
            return FALSE;
        }
#endif

        pVia->FBFreeStart = 0;
        pVia->FBFreeEnd = pVia->videoRambytes;

        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                   "Frame buffer start: %p, free start: 0x%x end: 0x%x\n",
                   pVia->FBBase, pVia->FBFreeStart, pVia->FBFreeEnd);
    }

#ifdef XSERVER_LIBPCIACCESS
    pScrn->memPhysBase = pVia->PciInfo->regions[0].base_addr;
#else
    pScrn->memPhysBase = pVia->PciInfo->memBase[0];
#endif
    pScrn->fbOffset = 0;
    if (pVia->IsSecondary)
        pScrn->fbOffset = pScrn->videoRam << 10;

    return TRUE;
}

void
VIAInitialize3DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int i;

    VIASETREG(VIA_REG_TRANSET, 0x00010000);
    for (i = 0; i <= 0x7D; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);

    VIASETREG(VIA_REG_TRANSET, 0x00020000);
    for (i = 0; i <= 0x94; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);

    VIASETREG(VIA_REG_TRANSET, 0x01020000);
    for (i = 0; i <= 0x94; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);

    VIASETREG(VIA_REG_TRANSET, 0xfe020000);
    for (i = 0; i <= 0x03; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);

    VIASETREG(VIA_REG_TRANSET, 0x00030000);
    for (i = 0; i <= 0xff; i++)
        VIASETREG(VIA_REG_TRANSPACE, 0);

    VIASETREG(VIA_REG_TRANSET, 0x00100000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00333004);
    VIASETREG(VIA_REG_TRANSPACE, 0x10000002);
    VIASETREG(VIA_REG_TRANSPACE, 0x60000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x61000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x62000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x63000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x64000000);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    if (pVia->Chipset == VIA_CLE266 && pVia->ChipRev >= 3)
        VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
    else
        VIASETREG(VIA_REG_TRANSPACE, 0x4000800f);
    VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x45080C04);
    VIASETREG(VIA_REG_TRANSPACE, 0x46800408);
    VIASETREG(VIA_REG_TRANSPACE, 0x50000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x51000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x52000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x53000000);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE, 0x08000001);
    VIASETREG(VIA_REG_TRANSPACE, 0x0A000183);
    VIASETREG(VIA_REG_TRANSPACE, 0x0B00019F);
    VIASETREG(VIA_REG_TRANSPACE, 0x0C00018B);
    VIASETREG(VIA_REG_TRANSPACE, 0x0D00019B);
    VIASETREG(VIA_REG_TRANSPACE, 0x0E000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x0F000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x10000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x11000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x20000000);
}

Bool
ums_create(ScrnInfoPtr pScrn)
{
    ScreenPtr pScreen = pScrn->pScreen;
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned long offset;
    BoxRec AvailFBArea;
    Bool ret = TRUE;
    long size;
    int maxY;

#ifdef XF86DRI
    if (pVia->directRenderingType == DRI_1) {
        pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart) >> 2;
        if ((pVia->driSize > (pVia->maxDriSize * 1024)) && pVia->maxDriSize > 0)
            pVia->driSize = pVia->maxDriSize * 1024;

        /* In the case of DRI we handle all VRAM by the DRI ioctls */
        if (pVia->useEXA)
            return TRUE;

        /* XAA has to use FBManager so we have to split the space with DRI */
        maxY = pScrn->virtualY + (pVia->driSize / pVia->Bpl);
    } else
#endif
        maxY = pVia->FBFreeEnd / pVia->Bpl;

    /* FBManager can't handle more than 32767 scan lines */
    if (maxY > 32767)
        maxY = 32767;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = maxY;
    pVia->FBFreeStart = (AvailFBArea.y2 + 1) * pVia->Bpl;

    /*
     *   Initialization of the XFree86 framebuffer manager is done via
     *   Bool xf86InitFBManager(ScreenPtr pScreen, BoxPtr FullBox)
     *   FullBox represents the area of the framebuffer that the manager
     *   is allowed to manage. This is typically a box with a width
     *   of pScrn->displayWidth and a height of as many lines as can be fit
     *   within the total video memory
     */
    ret = xf86InitFBManager(pScreen, &AvailFBArea);
    if (ret != TRUE)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86InitFBManager init failed\n");

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            "Frame Buffer From (%d,%d) To (%d,%d)\n",
            AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2, AvailFBArea.y2));

    offset = (pVia->FBFreeStart + pVia->Bpp - 1) / pVia->Bpp;
    size = pVia->FBFreeEnd / pVia->Bpp - offset;
    if (size > 0)
        xf86InitFBManagerLinear(pScreen, offset, size);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            "Using %d lines for offscreen memory.\n",
            AvailFBArea.y2 - pScrn->virtualY));
    return TRUE;
}

Bool
UMSPreInit(ScrnInfoPtr pScrn)
{
    MessageType from = X_PROBED;
    VIAPtr pVia = VIAPTR(pScrn);
    CARD8 videoRam;
    vgaHWPtr hwp;
#ifdef XSERVER_LIBPCIACCESS
    struct pci_device *vgaDevice = pci_device_find_by_slot(0, 0, 0, 3);
    struct pci_device *bridge = pci_device_find_by_slot(0, 0, 0, 0);
#endif
    int bMemSize = 0;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

    if (!vgaHWGetHWRec(pScrn))
        return FALSE;

#if 0
    /* Here we can alter the number of registers saved and restored by the
     * standard vgaHWSave and Restore routines.
     */
    vgaHWSetRegCounts(pScrn, VGA_NUM_CRTC, VGA_NUM_SEQ, VGA_NUM_GFX,
                      VGA_NUM_ATTR);
#endif
    hwp = VGAHWPTR(pScrn);

    /* Strange place to do it but it needs access to the VGA registers.*/
    if (pVia->Chipset == VIA_CLE266)
        ViaDoubleCheckCLE266Revision(pScrn);

    switch (pVia->Chipset) {
        case VIA_CLE266:
#ifdef XSERVER_LIBPCIACCESS
	    pci_device_cfg_read_u8(bridge, &videoRam, 0xE1);
#else
	    videoRam = pciReadByte(pciTag(0, 0, 0), 0xE1) & 0x70;
#endif
	    pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
            break;
        case VIA_KM400:
#ifdef XSERVER_LIBPCIACCESS
            pci_device_cfg_read_u8(bridge, &videoRam, 0xE1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 0), 0xE1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
	    /* Workaround for #177 (VRAM probing fail on P4M800) */
	    if (pScrn->videoRam < 16384) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
					"Memory size detection failed: using 16 MB.\n");
		pScrn->videoRam = 16 << 10;
            }
            break;
        case VIA_PM800:
        case VIA_VM800:
        case VIA_K8M800:
#ifdef XSERVER_LIBPCIACCESS
            pci_device_cfg_read_u8(vgaDevice, &videoRam, 0xA1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 3), 0xA1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
            break;
        case VIA_K8M890:
        case VIA_P4M890:
        case VIA_P4M900:
        case VIA_CX700:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
#ifdef XSERVER_LIBPCIACCESS
            pci_device_cfg_read_u8(vgaDevice, &videoRam, 0xA1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 3), 0xA1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 12;
            break;
        default:
            if (pScrn->videoRam < 16384 || pScrn->videoRam > 65536) {
                xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                           "Using old memory-detection method.\n");
                bMemSize = hwp->readSeq(hwp, 0x39);
                if (bMemSize > 16 && bMemSize <= 128)
                    pScrn->videoRam = (bMemSize + 1) << 9;
                else if (bMemSize > 0 && bMemSize < 31)
                    pScrn->videoRam = bMemSize << 12;
                else {
                    from = X_DEFAULT;
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                               "Memory size detection failed: using 16 MB.\n");
                    pScrn->videoRam = 16 << 10;
                }
            } else {
                from = X_DEFAULT;
                xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                           "No memory-detection done. Use VideoRAM option.\n");
            }
    }

    /*
     * PCI BAR are limited to 256 MB.
     */
    if (pScrn->videoRam > (256 << 10)) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                    "Cannot use more than 256 MB of VRAM.\n");
                    pScrn->videoRam = (256 << 10);
    }

    if (from == X_PROBED) {
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Probed amount of VideoRAM = %d kB\n", pScrn->videoRam);
    }

    /* Split the FB for SAMM. */
    /* FIXME: For now, split the FB into two equal sections.
     * This should be user-adjustable via a config option. */
    if (pVia->IsSecondary) {
        DevUnion *pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr pVia1;

        pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        pScrn->videoRam = pScrn->videoRam >> 1;
        pVIAEnt->pPrimaryScrn->videoRam = pScrn->videoRam;
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        pVia1->videoRambytes = pScrn->videoRam << 10;
        pVia->FrameBufferBase += (pScrn->videoRam << 10);
    }

    pVia->videoRambytes = pScrn->videoRam << 10;

    /* maybe throw in some more sanity checks here */
#ifndef XSERVER_LIBPCIACCESS
    pVia->PciTag = pciTag(pVia->PciInfo->bus, pVia->PciInfo->device,
                          pVia->PciInfo->func);
#endif

    /* Detect the amount of installed RAM */
    if (!VIAMapMMIO(pScrn))
        return FALSE;

    if (!VIAMapFB(pScrn))
        return FALSE;
    return TRUE;
}
