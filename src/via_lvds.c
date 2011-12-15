/*
 * Copyright 2007 The Openchrome Project [openchrome.org]
 * Copyright 1998-2007 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2007 S3 Graphics, Inc. All Rights Reserved.
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
 * Integrated LVDS power management functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "via_driver.h"
#include "via_vgahw.h"
#include "via_mode.h"
#include <unistd.h>

static ViaPanelModeRec ViaPanelNativeModes[] = {
    {640, 480},
    {800, 600},
    {1024, 768},
    {1280, 768},
    {1280, 1024},
    {1400, 1050},
    {1600, 1200},   /* 0x6 */
    {1280, 800},    /* 0x7 Resolution 1280x800 (Samsung NC20) */
    {800, 480},     /* 0x8 For Quanta 800x480 */
    {1024, 600},    /* 0x9 Resolution 1024x600 (for HP 2133) */
    {1366, 768},    /* 0xA Resolution 1366x768 */
    {1920, 1080},
    {1920, 1200},
    {1280, 1024},   /* 0xD */
    {1440, 900},    /* 0xE */
    {1280, 720},    /* 0xF 480x640 */
    {1200, 900},   /* 0x10 For OLPC 1.5 */
    {1360, 768},   /* 0x11 Resolution 1360X768 */
    {1024, 768},   /* 0x12 Resolution 1024x768 */
    {800, 480}     /* 0x13 General 8x4 panel use this setting */
};

/*
	1. Formula:
		2^13 X 0.0698uSec [1/14.318MHz] = 8192 X 0.0698uSec =572.1uSec
		Timer = Counter x 572 uSec
	2. Note:
		0.0698 uSec is too small to compute for hardware. So we multiply a
		reference value(2^13) to make it big enough to compute for hardware.
	3. Note:
		The meaning of the TD0~TD3 are count of the clock.
		TD(sec) = (sec)/(per clock) x (count of clocks)
*/

#define TD0 200
#define TD1 25
#define TD2 0
#define TD3 25

static void
ViaLVDSSoftwarePowerFirstSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSSoftwarePowerFirstSequence: %d\n", on));
    if (on) {

        /* Software control power sequence ON*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0x7F);
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x01);
        usleep(TD0);

        /* VDD ON*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x10);
        usleep(TD1);

        /* DATA ON */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x08);
        usleep(TD2);

        /* VEE ON (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x04);
        usleep(TD3);

        /* Back-Light ON */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) | 0x02);
    } else {
        /* Back-Light OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFD);
        usleep(TD3);

        /* VEE OFF (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFB);
        usleep(TD2);

        /* DATA OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xF7);
        usleep(TD1);

        /* VDD OFF */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xEF);
    }
}

static void
ViaLVDSSoftwarePowerSecondSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSSoftwarePowerSecondSequence: %d\n", on));
    if (on) {
        /* Secondary power hardware power sequence enable 0:off 1: on */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) & 0xFD);

        /* Software control power sequence ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x01);
        usleep(TD0);

        /* VDD ON*/
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x10);
        usleep(TD1);

        /* DATA ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x08);
        usleep(TD2);

        /* VEE ON (unused on vt3353)*/
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x04);
        usleep(TD3);

        /* Back-Light ON */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) | 0x02);
    } else {
        /* Back-Light OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFD);
        usleep(TD3);

        /* VEE OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFB);
        /* Delay TD2 msec. */
        usleep(TD2);

        /* DATA OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xF7);
        /* Delay TD1 msec. */
        usleep(TD1);

        /* VDD OFF */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xEF);
    }
}


static void
ViaLVDSHardwarePowerFirstSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (on) {
        /* Use hardware control power sequence. */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0xFE);
        /* Turn on back light. */
        hwp->writeCrtc(hwp, 0x91, hwp->readCrtc(hwp, 0x91) & 0x3F);
        /* Turn on hardware power sequence. */
        hwp->writeCrtc(hwp, 0x6A, hwp->readCrtc(hwp, 0x6A) | 0x08);
    } else {
        /* Turn off power sequence. */
        hwp->writeCrtc(hwp, 0x6A, hwp->readCrtc(hwp, 0x6A) & 0xF7);
        usleep(1);
        /* Turn off back light. */
        hwp->writeCrtc(hwp, 0x91, 0xC0);
    }
}

static void
ViaLVDSHardwarePowerSecondSequence(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    if (on) {
        /* Use hardware control power sequence. */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0xFE);
        /* Turn on back light. */
        hwp->writeCrtc(hwp, 0xD3, hwp->readCrtc(hwp, 0xD3) & 0x3F);
        /* Turn on hardware power sequence. */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) | 0x02);
    } else {
        /* Turn off power sequence. */
        hwp->writeCrtc(hwp, 0xD4, hwp->readCrtc(hwp, 0xD4) & 0xFD);
        usleep(1);
        /* Turn off back light. */
        hwp->writeCrtc(hwp, 0xD3, 0xC0);
    }
}

static void
ViaLVDSDFPPower(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);

    /* Switch DFP High/Low pads on or off for channels active at EnterVT(). */
    ViaSeqMask(hwp, 0x2A, on ? pVia->SavedReg.SR2A : 0, 0x0F);
}

static void
ViaLVDSPowerChannel(ScrnInfoPtr pScrn, Bool on)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 lvdsMask;

    if (on) {
        /* LVDS0: 0x7F, LVDS1: 0xBF */
        lvdsMask = 0x7F & 0xBF;
        hwp->writeCrtc(hwp, 0xD2, hwp->readCrtc(hwp, 0xD2) & lvdsMask);
    } else {
        /* LVDS0: 0x80, LVDS1: 0x40 */
        lvdsMask = 0x80 | 0x40;
        hwp->writeCrtc(hwp, 0xD2, hwp->readCrtc(hwp, 0xD2) | lvdsMask);
    }
}

static void
ViaLVDSPower(ScrnInfoPtr pScrn, Bool on)
{
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaLVDSPower %d\n", on));
    VIAPtr pVia = VIAPTR(pScrn);

    /*
     * VX800, CX700 have HW issue, so we'd better use SW power sequence
     * Fix Ticket #308
     */
    switch (pVia->Chipset) {
        case VIA_VX800:
        case VIA_CX700:
            ViaLVDSSoftwarePowerFirstSequence(pScrn, on);
            ViaLVDSSoftwarePowerSecondSequence(pScrn, on);
            break;
        default:
            ViaLVDSHardwarePowerFirstSequence(pScrn, on);
            ViaLVDSHardwarePowerSecondSequence(pScrn, on);
    }

    ViaLVDSDFPPower(pScrn, on);
    ViaLVDSPowerChannel(pScrn, on);
}

static void
via_lvds_create_resources(xf86OutputPtr output)
{
}

static Bool
via_lvds_set_property(xf86OutputPtr output, Atom property,
						RRPropertyValuePtr value)
{
	return FALSE;
}

static Bool
via_lvds_get_property(xf86OutputPtr output, Atom property)
{
	return FALSE;
}

static void
via_lvds_dpms(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	if (pVia->pVbe) {
		ViaVbePanelPower(pVia->pVbe, (mode == DPMSModeOn));
	} else {
		switch (mode) {
		case DPMSModeOn:
			switch (pVia->Chipset) {
			case VIA_P4M900:
			case VIA_CX700:
			case VIA_VX800:
			case VIA_VX855:
			case VIA_VX900:
				ViaLVDSPower(pScrn, TRUE);
				break;
			}
			ViaLCDPower(pScrn, TRUE);
			break;

		case DPMSModeStandby:
		case DPMSModeSuspend:
		case DPMSModeOff:
			switch (pVia->Chipset) {
			case VIA_P4M900:
			case VIA_CX700:
			case VIA_VX800:
			case VIA_VX855:
			case VIA_VX900:
				ViaLVDSPower(pScrn, FALSE);
				break;
			}
			ViaLCDPower(pScrn, FALSE);
			break;
		}
	}
}

static void
via_lvds_save(xf86OutputPtr output)
{
}

static void
via_lvds_restore(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;

	ViaLCDPower(pScrn, TRUE);
}

/*
 * Try to interpret EDID ourselves.
 */
static Bool
ViaPanelGetSizeFromEDID(ScrnInfoPtr pScrn, xf86MonPtr pMon,
                        int *width, int *height)
{
    int i, max_hsize = 0, vsize = 0;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetPanelSizeFromEDID\n"));

    /* !!! Why are we not checking VESA modes? */

    /* checking standard timings */
    for (i = 0; i < STD_TIMINGS; i++)
        if ((pMon->timings2[i].hsize > 256)
            && (pMon->timings2[i].hsize > max_hsize)) {
            max_hsize = pMon->timings2[i].hsize;
            vsize = pMon->timings2[i].vsize;
        }

    if (max_hsize != 0) {
        *width = max_hsize;
        *height = vsize;
        return TRUE;
    }

    /* checking detailed monitor section */

    /* !!! skip Ranges and standard timings */

    /* check detailed timings */
    for (i = 0; i < DET_TIMINGS; i++)
        if (pMon->det_mon[i].type == DT) {
            struct detailed_timings timing = pMon->det_mon[i].section.d_timings;

            /* ignore v_active for now */
            if ((timing.clock > 15000000) && (timing.h_active > max_hsize)) {
                max_hsize = timing.h_active;
                vsize = timing.v_active;
            }
        }

    if (max_hsize != 0) {
        *width = max_hsize;
        *height = vsize;
        return TRUE;
    }
    return FALSE;
}

static Bool
ViaPanelGetSizeFromDDCv1(ScrnInfoPtr pScrn, int *width, int *height)
{
    VIAPtr pVia = VIAPTR(pScrn);
    xf86MonPtr pMon;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetPanelSizeFromDDCv1\n"));

    if (!xf86I2CProbeAddress(pVia->pI2CBus2, 0xA0))
        return FALSE;

    pMon = xf86DoEEDID(pScrn->scrnIndex, pVia->pI2CBus2, TRUE);
    if (!pMon)
        return FALSE;

    pVia->DDC2 = pMon;

    if (!pVia->DDC1) {
        xf86PrintEDID(pMon);
        xf86SetDDCproperties(pScrn, pMon);
    }

    if (!ViaPanelGetSizeFromEDID(pScrn, pMon, width, height)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to read PanelSize from EDID information\n");
        return FALSE;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "VIAGetPanelSizeFromDDCv1: (%dx%d)\n", *width, *height));
    return TRUE;
}

static Bool
ViaPanelGetSizeFromDDCv2(ScrnInfoPtr pScrn, int *width)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD8 W_Buffer[1];
    CARD8 R_Buffer[4];
    I2CDevPtr dev;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetPanelSizeFromDDCv2\n"));

    if (!xf86I2CProbeAddress(pVia->pI2CBus2, 0xA2))
        return FALSE;

    dev = xf86CreateI2CDevRec();
    if (!dev)
        return FALSE;

    dev->DevName = "EDID2";
    dev->SlaveAddr = 0xA2;
    dev->ByteTimeout = 2200;  /* VESA DDC spec 3 p. 43 (+10 %) */
    dev->StartTimeout = 550;
    dev->BitTimeout = 40;
    dev->ByteTimeout = 40;
    dev->AcknTimeout = 40;
    dev->pI2CBus = pVia->pI2CBus2;

    if (!xf86I2CDevInit(dev)) {
        xf86DestroyI2CDevRec(dev, TRUE);
        return FALSE;
    }

    xf86I2CReadByte(dev, 0x00, R_Buffer);
    if (R_Buffer[0] != 0x20) {
        xf86DestroyI2CDevRec(dev, TRUE);
        return FALSE;
    }

    /* Found EDID2 Table */

    W_Buffer[0] = 0x76;
    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 2);
    xf86DestroyI2CDevRec(dev, TRUE);

    *width = R_Buffer[0] | (R_Buffer[1] << 8);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "VIAGetPanelSizeFromDDCv2: %d\n", *width));
    return TRUE;
}

static Bool
ViaGetResolutionIndex(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VIABIOSInfoPtr pBIOSInfo = VIAPTR(pScrn)->pBIOSInfo;
    int i;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaGetResolutionIndex: Looking for %dx%d\n",
                     mode->CrtcHDisplay, mode->CrtcVDisplay));
    for (i = 0; ViaResolutionTable[i].Index != VIA_RES_INVALID; i++) {
        if ((ViaResolutionTable[i].X == mode->CrtcHDisplay)
            && (ViaResolutionTable[i].Y == mode->CrtcVDisplay)) {
            pBIOSInfo->ResolutionIndex = ViaResolutionTable[i].Index;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaGetResolutionIndex:"
                             " %d\n", pBIOSInfo->ResolutionIndex));
            return TRUE;
        }
    }

    pBIOSInfo->ResolutionIndex = VIA_RES_INVALID;
    return FALSE;
}

static int
ViaGetVesaMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    int i;

    for (i = 0; ViaVesaModes[i].Width; i++)
        if ((ViaVesaModes[i].Width == mode->CrtcHDisplay)
            && (ViaVesaModes[i].Height == mode->CrtcVDisplay)) {
            switch (pScrn->bitsPerPixel) {
                case 8:
                    return ViaVesaModes[i].mode_8b;
                case 16:
                    return ViaVesaModes[i].mode_16b;
                case 24:
                case 32:
                    return ViaVesaModes[i].mode_32b;
                default:
                    return 0xFFFF;
            }
        }
    return 0xFFFF;
}

static void
VIAGetPanelSize(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    char *PanelSizeString[7] = { "640x480", "800x480", "800x600", "1024x768", "1280x768"
                                 "1280x1024", "1400x1050", "1600x1200" };
    int width = 0;
    int height = 0;
    Bool ret;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAGetPanelSize (UseLegacyModeSwitch)\n"));

    ret = ViaPanelGetSizeFromDDCv1(pScrn, &width, &height);
    if (!ret)
        ret = ViaPanelGetSizeFromDDCv2(pScrn, &width);

    if (ret) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID returned resolution %d x %d \n", width, height));
        switch (width) {
            case 640:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL6X4;
                break;
            case 800:
                if (height == 480)
                    pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL8X4;
                else
                    pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL8X6;
                break;
            case 1024:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL10X7;
                break;
            case 1280:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL12X10;
                break;
            case 1400:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL14X10;
                break;
            case 1600:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL16X12;
                break;
            default:
                pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL_INVALID;
                break;
        }
    } else {
        pBIOSInfo->Panel->NativeModeIndex = hwp->readCrtc(hwp, 0x3F) >> 4;
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Unable to get information from EDID. Resolution from Scratchpad: %d \n", pBIOSInfo->Panel->NativeModeIndex));
        if (pBIOSInfo->Panel->NativeModeIndex == 0) {
            /* VIA_PANEL6X4 == 0, but that value equals unset */
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unable to "
                       "retrieve PanelSize: using default (1024x768)\n");
            pBIOSInfo->Panel->NativeModeIndex = VIA_PANEL10X7;
            return;
        }
    }

    if (pBIOSInfo->Panel->NativeModeIndex < 7)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using panel at %s.\n",
                   PanelSizeString[pBIOSInfo->Panel->NativeModeIndex]);
    else
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unknown panel size "
                   "detected: %d.\n", pBIOSInfo->Panel->NativeModeIndex);
}

/*
 *
 * ViaResolutionTable[i].PanelIndex is pBIOSInfo->PanelSize
 * pBIOSInfo->PanelIndex is the index to lcdTable.
 *
 */
static Bool
ViaPanelGetIndex(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VIABIOSInfoPtr pBIOSInfo = VIAPTR(pScrn)->pBIOSInfo;
    int i;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex\n"));

    pBIOSInfo->PanelIndex = VIA_BIOS_NUM_PANEL;

    if (pBIOSInfo->Panel->NativeModeIndex == VIA_PANEL_INVALID) {
        VIAGetPanelSize(pScrn);
        if (pBIOSInfo->Panel->NativeModeIndex == VIA_PANEL_INVALID) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "ViaPanelGetIndex: PanelSize not set.\n");
            return FALSE;
        }
    }

    if (!ViaGetResolutionIndex(pScrn, mode)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel does not support this"
                   " resolution: %s\n", mode->name);
        return FALSE;
    }

    for (i = 0; ViaResolutionTable[i].Index != VIA_RES_INVALID; i++) {
        if (ViaResolutionTable[i].PanelIndex
            == pBIOSInfo->Panel->NativeModeIndex) {
            pBIOSInfo->Panel->NativeMode->Width = ViaResolutionTable[i].X;
            pBIOSInfo->Panel->NativeMode->Height = ViaResolutionTable[i].Y;
            break;
        }
    }

    if (ViaResolutionTable[i].Index == VIA_RES_INVALID) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex: Unable"
                   " to find matching PanelSize in ViaResolutionTable.\n");
        return FALSE;
    }

    if ((pBIOSInfo->Panel->NativeMode->Width != mode->CrtcHDisplay) ||
        (pBIOSInfo->Panel->NativeMode->Height != mode->CrtcVDisplay)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex: Non-native"
                   " resolutions are broken.\n");
        return FALSE;
    }

    for (i = 0; i < VIA_BIOS_NUM_PANEL; i++) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex:"
                         "Match Debug: %d == %d)\n", pBIOSInfo->Panel->NativeModeIndex,
                         lcdTable[i].fpSize));
        if (lcdTable[i].fpSize == pBIOSInfo->Panel->NativeModeIndex) {
            int modeNum, tmp;

            modeNum = ViaGetVesaMode(pScrn, mode);
            if (modeNum == 0xFFFF) {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "ViaPanelGetIndex: "
                           "Unable to determine matching VESA modenumber.\n");
                return FALSE;
            }

            tmp = 0x01 << (modeNum & 0xF);
            if ((CARD16) tmp & lcdTable[i].SuptMode[(modeNum >> 4)]) {
                pBIOSInfo->PanelIndex = i;
                DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex:"
                                 "index: %d (%dx%d)\n", pBIOSInfo->PanelIndex,
                                 pBIOSInfo->Panel->NativeMode->Width, pBIOSInfo->Panel->NativeMode->Height));
                return TRUE;
            }

            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex: Unable"
                       " to match given mode with this PanelSize.\n");
            return FALSE;
        }
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelGetIndex: Unable"
               " to match PanelSize with an lcdTable entry.\n");
    return FALSE;
}

static int
via_lvds_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	ScrnInfoPtr pScrn = output->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	if (pVia->UseLegacyModeSwitch) {
		if (!ViaPanelGetIndex(pScrn, pMode))
			return MODE_BAD;
	} else {
		ViaPanelModePtr nativeMode = pVia->pBIOSInfo->Panel->NativeMode;

		if (nativeMode->Width < pMode->HDisplay ||
			nativeMode->Height < pMode->VDisplay)
			return MODE_PANEL;

		if (!ViaModeDotClockTranslate(pScrn, pMode))
			return MODE_NOCLOCK;
	}
	return MODE_OK;
}

static Bool
via_lvds_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
					DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
via_lvds_prepare(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;

    ViaLCDPower(pScrn, FALSE);
}

static void
via_lvds_commit(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;

    ViaLCDPower(pScrn, TRUE);
}

/*
 * Broken, only does native mode decently. I (Luc) personally broke this.
 */
static void
VIASetLCDMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    VIALCDModeTableRec Table = lcdTable[pBIOSInfo->PanelIndex];
    CARD8 modeNum = 0;
    int resIdx;
    int port, offset, data;
    int i, j, misc;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIASetLCDMode\n"));

    if (pBIOSInfo->Panel->NativeModeIndex == VIA_PANEL12X10)
        hwp->writeCrtc(hwp, 0x89, 0x07);

    /* LCD Expand Mode Y Scale Flag */
    pBIOSInfo->scaleY = FALSE;

    /* Set LCD InitTb Regs */
    if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
        if (pVia->IsSecondary)
            pBIOSInfo->Clock = Table.InitTb.LCDClk_12Bit;
        else {
            pBIOSInfo->Clock = Table.InitTb.VClk_12Bit;
            /* for some reason still to be defined this is necessary */
            ViaSetSecondaryDotclock(pScrn, Table.InitTb.LCDClk_12Bit);
        }
    } else {
        if (pVia->IsSecondary)
            pBIOSInfo->Clock = Table.InitTb.LCDClk;
        else {
            pBIOSInfo->Clock = Table.InitTb.VClk;
            ViaSetSecondaryDotclock(pScrn, Table.InitTb.LCDClk);
        }

    }

    ViaSetUseExternalClock(hwp);

    for (i = 0; i < Table.InitTb.numEntry; i++) {
        port = Table.InitTb.port[i];
        offset = Table.InitTb.offset[i];
        data = Table.InitTb.data[i];
        ViaVgahwWrite(hwp, 0x300 + port, offset, 0x301 + port, data);
    }

    if ((mode->CrtcHDisplay != pBIOSInfo->Panel->NativeMode->Width)
        || (mode->CrtcVDisplay != pBIOSInfo->Panel->NativeMode->Height)) {
        VIALCDModeEntryPtr Main;
        VIALCDMPatchEntryPtr Patch1, Patch2;
        int numPatch1, numPatch2;

        resIdx = VIA_RES_INVALID;

        /* Find MxxxCtr & MxxxExp Index and
         * HWCursor Y Scale (PanelSize Y / Res. Y) */
        pBIOSInfo->resY = mode->CrtcVDisplay;
        switch (pBIOSInfo->ResolutionIndex) {
            case VIA_RES_640X480:
                resIdx = 0;
                break;
            case VIA_RES_800X600:
                resIdx = 1;
                break;
            case VIA_RES_1024X768:
                resIdx = 2;
                break;
            case VIA_RES_1152X864:
                resIdx = 3;
                break;
            case VIA_RES_1280X768:
            case VIA_RES_1280X960:
            case VIA_RES_1280X1024:
                if (pBIOSInfo->Panel->NativeModeIndex == VIA_PANEL12X10)
                    resIdx = VIA_RES_INVALID;
                else
                    resIdx = 4;
                break;
            default:
                resIdx = VIA_RES_INVALID;
                break;
        }

        if ((mode->CrtcHDisplay == 640) && (mode->CrtcVDisplay == 400))
            resIdx = 0;

        if (resIdx == VIA_RES_INVALID) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "VIASetLCDMode: Failed "
                       "to find a suitable Panel Size index.\n");
            return;
        }

        if (pBIOSInfo->Center) {
            Main = &(Table.MCtr[resIdx]);
            Patch1 = Table.MPatchDP1Ctr;
            numPatch1 = Table.numMPatchDP1Ctr;
            Patch2 = Table.MPatchDP2Ctr;
            numPatch2 = Table.numMPatchDP2Ctr;
        } else {  /* expand! */
            /* LCD Expand Mode Y Scale Flag */
            pBIOSInfo->scaleY = TRUE;
            Main = &(Table.MExp[resIdx]);
            Patch1 = Table.MPatchDP1Exp;
            numPatch1 = Table.numMPatchDP1Exp;
            Patch2 = Table.MPatchDP2Exp;
            numPatch2 = Table.numMPatchDP2Exp;
        }

        /* Set Main LCD Registers */
        for (i = 0; i < Main->numEntry; i++) {
            ViaVgahwWrite(hwp, 0x300 + Main->port[i], Main->offset[i],
                          0x301 + Main->port[i], Main->data[i]);
        }

        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            if (pVia->IsSecondary)
                pBIOSInfo->Clock = Main->LCDClk_12Bit;
            else
                pBIOSInfo->Clock = Main->VClk_12Bit;
        } else {
            if (pVia->IsSecondary)
                pBIOSInfo->Clock = Main->LCDClk;
            else
                pBIOSInfo->Clock = Main->VClk;
        }

        j = ViaGetVesaMode(pScrn, mode);
        if (j == 0xFFFF) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "VIASetLCDMode: "
                       "Unable to determine matching VESA modenumber.\n");
            return;
        }
        for (i = 0; i < modeFix.numEntry; i++) {
            if (modeFix.reqMode[i] == j) {
                modeNum = modeFix.fixMode[i];
                break;
            }
        }

        /* Set LCD Mode patch registers. */
        for (i = 0; i < numPatch2; i++, Patch2++) {
            if (Patch2->Mode == modeNum) {
                if (!pBIOSInfo->Center
                    && (mode->CrtcHDisplay == pBIOSInfo->Panel->NativeMode->Width))
                    pBIOSInfo->scaleY = FALSE;

                for (j = 0; j < Patch2->numEntry; j++) {
                    ViaVgahwWrite(hwp, 0x300 + Patch2->port[j],
                                  Patch2->offset[j], 0x301 + Patch2->port[j],
                                  Patch2->data[j]);
                }

                if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
                    if (pVia->IsSecondary)
                        pBIOSInfo->Clock = Patch2->LCDClk_12Bit;
                    else
                        pBIOSInfo->Clock = Patch2->VClk_12Bit;
                } else {
                    if (pVia->IsSecondary)
                        pBIOSInfo->Clock = Patch2->LCDClk;
                    else
                        pBIOSInfo->Clock = Patch2->VClk;
                }
                break;
            }
        }

        /* Set LCD Secondary Mode Patch registers. */
        if (pVia->IsSecondary) {
            for (i = 0; i < numPatch1; i++, Patch1++) {
                if (Patch1->Mode == modeNum) {
                    for (j = 0; j < Patch1->numEntry; j++) {
                        ViaVgahwWrite(hwp, 0x300 + Patch1->port[j],
                                      Patch1->offset[j],
                                      0x301 + Patch1->port[j], Patch1->data[j]);
                    }
                    break;
                }
            }
        }
    }

    /* LCD patch 3D5.02 */
    misc = hwp->readCrtc(hwp, 0x01);
    hwp->writeCrtc(hwp, 0x02, misc);

    /* Enable LCD */
    if (!pVia->IsSecondary) {
        /* CRT Display Source Bit 6 - 0: CRT, 1: LCD */
        ViaSeqMask(hwp, 0x16, 0x40, 0x40);

        /* Enable Simultaneous */
        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            hwp->writeCrtc(hwp, 0x6B, 0xA8);

            if ((pVia->Chipset == VIA_CLE266)
                && CLE266_REV_IS_AX(pVia->ChipRev))
                hwp->writeCrtc(hwp, 0x93, 0xB1);
            else
                hwp->writeCrtc(hwp, 0x93, 0xAF);
        } else {
            ViaCrtcMask(hwp, 0x6B, 0x08, 0x08);
            hwp->writeCrtc(hwp, 0x93, 0x00);
        }
        hwp->writeCrtc(hwp, 0x6A, 0x48);
    } else {
        /* CRT Display Source Bit 6 - 0: CRT, 1: LCD */
        ViaSeqMask(hwp, 0x16, 0x00, 0x40);

        /* Enable SAMM */
        if (pBIOSInfo->BusWidth == VIA_DI_12BIT) {
            ViaCrtcMask(hwp, 0x6B, 0x20, 0x20);
            if ((pVia->Chipset == VIA_CLE266)
                && CLE266_REV_IS_AX(pVia->ChipRev))
                hwp->writeCrtc(hwp, 0x93, 0xB1);
            else
                hwp->writeCrtc(hwp, 0x93, 0xAF);
        } else {
            hwp->writeCrtc(hwp, 0x6B, 0x00);
            hwp->writeCrtc(hwp, 0x93, 0x00);
        }
        hwp->writeCrtc(hwp, 0x6A, 0xC8);
    }
}

static void
ViaPanelCenterMode(DisplayModePtr centerMode, DisplayModePtr panelMode,
                   DisplayModePtr mode)
{
    memcpy(centerMode, mode, sizeof(DisplayModeRec));

    CARD32 HDiff = (panelMode->CrtcHDisplay - mode->CrtcHDisplay) / 2;
    CARD32 VDiff = (panelMode->CrtcVDisplay - mode->CrtcVDisplay) / 2;

    centerMode->CrtcHTotal += HDiff * 2;
    centerMode->CrtcVTotal += VDiff * 2;

    centerMode->CrtcHSyncStart += HDiff;
    centerMode->CrtcHSyncEnd += HDiff;
    centerMode->CrtcHBlankStart += HDiff;
    centerMode->CrtcHBlankEnd += HDiff;

    centerMode->CrtcVSyncStart += VDiff;
    centerMode->CrtcVSyncEnd += VDiff;
    centerMode->CrtcVBlankStart += VDiff;
    centerMode->CrtcVBlankEnd += VDiff;
}

static void
ViaPanelScale(ScrnInfoPtr pScrn, int resWidth, int resHeight,
              int panelWidth, int panelHeight)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    int horScalingFactor = 0;
    int verScalingFactor = 0;
    CARD8 cra2 = 0;
    CARD8 cr77 = 0;
    CARD8 cr78 = 0;
    CARD8 cr79 = 0;
    CARD8 cr9f = 0;
    Bool scaling = FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                    "ViaPanelScale: %d,%d -> %d,%d\n",
                    resWidth, resHeight, panelWidth, panelHeight));

    if (resWidth < panelWidth) {
        /* Load Horizontal Scaling Factor */
        if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400) {
            horScalingFactor = ((resWidth - 1) * 4096) / (panelWidth - 1);

            /* Horizontal scaling enabled */
            cra2 = 0xC0;
            cr9f = horScalingFactor & 0x0003;   /* HSCaleFactor[1:0] at CR9F[1:0] */
        } else {
            /* TODO: Need testing */
            horScalingFactor = ((resWidth - 1) * 1024) / (panelWidth - 1);
        }

        cr77 = (horScalingFactor & 0x03FC) >> 2;   /* HSCaleFactor[9:2] at CR77[7:0] */
        cr79 = (horScalingFactor & 0x0C00) >> 10;  /* HSCaleFactor[11:10] at CR79[5:4] */
        cr79 <<= 4;
        scaling = TRUE;
    }

    if (resHeight < panelHeight) {
        /* Load Vertical Scaling Factor */
        if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400) {
            verScalingFactor = ((resHeight - 1) * 2048) / (panelHeight - 1);

            /* Vertical scaling enabled */
            cra2 |= 0x08;
            cr79 |= ((verScalingFactor & 0x0001) << 3); /* VSCaleFactor[0] at CR79[3] */
        } else {
            /* TODO: Need testing */
            verScalingFactor = ((resHeight - 1) * 1024) / (panelHeight - 1);
        }

        cr78 |= (verScalingFactor & 0x01FE) >> 1;           /* VSCaleFactor[8:1] at CR78[7:0] */

        cr79 |= ((verScalingFactor & 0x0600) >> 9) << 6;    /* VSCaleFactor[10:9] at CR79[7:6] */
        scaling = TRUE;
    }

    if (scaling) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                "Scaling factor: horizontal %d (0x%x), vertical %d (0x%x)\n",
        horScalingFactor, horScalingFactor,
        verScalingFactor, verScalingFactor));

        ViaCrtcMask(hwp, 0x77, cr77, 0xFF);
        ViaCrtcMask(hwp, 0x78, cr78, 0xFF);
        ViaCrtcMask(hwp, 0x79, cr79, 0xF8);

        if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400) {
            ViaCrtcMask(hwp, 0x9F, cr9f, 0x03);
        }
        ViaCrtcMask(hwp, 0x79, 0x03, 0x03);
    } else {
        /*  Disable panel scale */
        ViaCrtcMask(hwp, 0x79, 0x00, 0x01);
    }

    if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400) {
        ViaCrtcMask(hwp, 0xA2, cra2, 0xC8);
    }

    /* Horizontal scaling selection: interpolation */
    // ViaCrtcMask(hwp, 0x79, 0x02, 0x02);
    // else
    // ViaCrtcMask(hwp, 0x79, 0x00, 0x02);
    /* Horizontal scaling factor selection original / linear */
    //ViaCrtcMask(hwp, 0xA2, 0x40, 0x40);
}

static void
ViaPanelScaleDisable(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    ViaCrtcMask(hwp, 0x79, 0x00, 0x01);
    /* Disable VX900 down scaling */
    if (pVia->Chipset == VIA_VX900)
        ViaCrtcMask(hwp, 0x89, 0x00, 0x01);
    if (pVia->Chipset != VIA_CLE266 && pVia->Chipset != VIA_KM400)
        ViaCrtcMask(hwp, 0xA2, 0x00, 0xC8);
}

static void
via_lvds_mode_set(xf86OutputPtr  output, DisplayModePtr mode,
					DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    VIAPtr pVia = VIAPTR(pScrn);

	/*
	 * FIXME: pVia->IsSecondary is not working here.  We should be able
	 * to detect when the display is using the secondary head.
	 * TODO: This should be enabled for other chipsets as well.
	 */
	if (pVia->pVbe) {
		if (!pVia->useLegacyVBE) {
			VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

			/*
			 * FIXME: Should we always set the panel expansion?
			 * Does it depend on the resolution?
			 */
			if (!ViaVbeSetPanelMode(pScrn, !pBIOSInfo->Center)) {
				xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
							"Unable to set the panel mode.\n");
			}
		}

		switch (pVia->Chipset) {
		case VIA_P4M900:
		case VIA_VX800:
		case VIA_VX855:
		case VIA_VX900:
			/*
			 * Since we are using virtual, we need to adjust
			 * the offset to match the framebuffer alignment.
			 */
			if (pScrn->displayWidth != adjusted_mode->CrtcHDisplay)
				ViaSecondCRTCHorizontalOffset(pScrn);
			break;
		}
    } else {
        VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

        if (!pVia->UseLegacyModeSwitch) {
            DisplayModePtr nativeDisplayMode = pBIOSInfo->Panel->NativeDisplayMode;
            DisplayModePtr centeredMode = pBIOSInfo->Panel->CenteredMode;
            DisplayModePtr realMode = adjusted_mode;

            if (nativeDisplayMode) {
                ViaPanelScale(pScrn, mode->HDisplay, mode->VDisplay,
                                nativeDisplayMode->HDisplay,
                                nativeDisplayMode->VDisplay);
                if (!pBIOSInfo->Center &&
                   (mode->HDisplay < nativeDisplayMode->HDisplay ||
                    mode->VDisplay < nativeDisplayMode->VDisplay)) {
                    pBIOSInfo->Panel->Scale = TRUE;
                    realMode = nativeDisplayMode;
                } else {
                    pBIOSInfo->Panel->Scale = FALSE;
                    ViaPanelCenterMode(centeredMode, nativeDisplayMode, mode);
                    realMode = centeredMode;
                    ViaPanelScaleDisable(pScrn);
                }
            }
        } else {
            xf86CrtcPtr crtc = output->crtc;
            ViaCRTCInfoPtr iga = crtc->driver_private;

            if (iga->index) {
                /* IGA 2 */
                if (pBIOSInfo->PanelIndex != VIA_BIOS_NUM_PANEL) {
                    pBIOSInfo->SetDVI = TRUE;
                    VIASetLCDMode(pScrn, mode);
                }
            } else {
                /* IGA 1 */
                if (ViaPanelGetIndex(pScrn, adjusted_mode))
                    VIASetLCDMode(pScrn, adjusted_mode);
            }
        }
    }
}

/*
 * Generates a display mode for the native panel resolution,  using CVT.
 */
static void
ViaPanelGetNativeDisplayMode(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaPanelModePtr panelMode = pVia->pBIOSInfo->Panel->NativeMode;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                    "ViaPanelGetNativeDisplayMode\n"));

    if (panelMode->Width && panelMode->Height) {
        /* TODO: fix refresh rate */
        DisplayModePtr p = xf86CVTMode(panelMode->Width, panelMode->Height,
                                        60.0f, TRUE, FALSE);
        if (p) {

        p->CrtcHDisplay = p->HDisplay;
        p->CrtcHSyncStart = p->HSyncStart;
        p->CrtcHSyncEnd = p->HSyncEnd;
        p->CrtcHTotal = p->HTotal;
        p->CrtcHSkew = p->HSkew;
        p->CrtcVDisplay = p->VDisplay;
        p->CrtcVSyncStart = p->VSyncStart;
        p->CrtcVSyncEnd = p->VSyncEnd;
        p->CrtcVTotal = p->VTotal;

        p->CrtcVBlankStart = min(p->CrtcVSyncStart, p->CrtcVDisplay);
        p->CrtcVBlankEnd = max(p->CrtcVSyncEnd, p->CrtcVTotal);
        p->CrtcHBlankStart = min(p->CrtcHSyncStart, p->CrtcHDisplay);
        p->CrtcHBlankEnd = max(p->CrtcHSyncEnd, p->CrtcHTotal);
        p->type = M_T_DRIVER | M_T_PREFERRED;

        pVia->pBIOSInfo->Panel->NativeDisplayMode = p;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Out of memory. Size: %d bytes\n", sizeof(DisplayModeRec));
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                    "Invalid panel dimension (%dx%d)\n", panelMode->Width,
                    panelMode->Height);
    }
}

/*
 * Gets the native panel resolution from scratch pad registers.
 */
static void
ViaPanelGetNativeModeFromScratchPad(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 index;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaPanelGetNativeModeFromScratchPad\n"));

    index = hwp->readCrtc(hwp, 0x3F) & 0x0F;

    ViaPanelInfoPtr panel = pVia->pBIOSInfo->Panel;

    panel->NativeModeIndex = index;
    panel->NativeMode->Width = ViaPanelNativeModes[index].Width;
    panel->NativeMode->Height = ViaPanelNativeModes[index].Height;
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "Native Panel Resolution is %dx%d\n",
               panel->NativeMode->Width, panel->NativeMode->Height);
}

static int
ViaPanelLookUpModeIndex(int width, int height)
{
    int i, index = VIA_PANEL_INVALID;
    int length = sizeof(ViaPanelNativeModes) / sizeof(ViaPanelModeRec);


    for (i = 0; i < length; i++) {
        if (ViaPanelNativeModes[i].Width == width
            && ViaPanelNativeModes[i].Height == height) {
            index = i;
            break;
        }
    }
    return index;
}

static xf86OutputStatus
ViaPanelPreInit(ScrnInfoPtr pScrn)
{
    xf86OutputStatus status = XF86OutputStatusDisconnected;
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    ViaPanelInfoPtr panel = pBIOSInfo->Panel;

    if (!pVia->UseLegacyModeSwitch) {
        /* First try to get the mode from EDID. */
        if (panel->NativeModeIndex == VIA_PANEL_INVALID) {
            int width, height;
            Bool ret;

            ret = ViaPanelGetSizeFromDDCv1(pScrn, &width, &height);
            if (ret) {
                panel->NativeModeIndex = ViaPanelLookUpModeIndex(width, height);                DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ViaPanelLookUpModeIndex, Width %d, Height %d, NativeModeIndex%d\n", width, height, panel->NativeModeIndex));
                if (panel->NativeModeIndex != VIA_PANEL_INVALID) {
                    panel->NativeMode->Width = width;
                    panel->NativeMode->Height = height;
                    status = XF86OutputStatusConnected;
                }
            } else {
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Unable to get panel size from EDID. Return code: %d\n", ret);
            }
        }

        if (panel->NativeModeIndex == VIA_PANEL_INVALID)
            ViaPanelGetNativeModeFromScratchPad(pScrn);

        if (panel->NativeModeIndex != VIA_PANEL_INVALID) {
            ViaPanelGetNativeDisplayMode(pScrn);
            status = XF86OutputStatusConnected;
        }
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NativeModeIndex: %d\n", panel->NativeModeIndex )) ;
    }
    return status;
}

static xf86OutputStatus
via_lvds_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;

	return ViaPanelPreInit(pScrn);
}

static DisplayModePtr
via_lvds_get_modes(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	VIAPtr pVia = VIAPTR(pScrn);

	ViaPanelGetNativeDisplayMode(pScrn);
	return pVia->pBIOSInfo->Panel->NativeDisplayMode;
}

static void
via_lvds_destroy(xf86OutputPtr output)
{
}

static const xf86OutputFuncsRec via_lvds_funcs = {
	.create_resources	= via_lvds_create_resources,
	.set_property		= via_lvds_set_property,
	.get_property		= via_lvds_get_property,
	.dpms				= via_lvds_dpms,
	.save				= via_lvds_save,
	.restore			= via_lvds_restore,
	.mode_valid			= via_lvds_mode_valid,
	.mode_fixup			= via_lvds_mode_fixup,
	.prepare			= via_lvds_prepare,
	.commit				= via_lvds_commit,
	.mode_set			= via_lvds_mode_set,
	.detect				= via_lvds_detect,
	.get_modes			= via_lvds_get_modes,
	.destroy			= via_lvds_destroy,
};

/*
 * Sets the panel dimensions from the configuration
 * using name with format "9999x9999".
 */
void
ViaPanelGetNativeModeFromOption(ScrnInfoPtr pScrn, char *name)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    ViaPanelInfoPtr panel = pBIOSInfo->Panel;
    CARD8 index;
    CARD8 length;
    char aux[10];

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "ViaPanelGetNativeModeFromOption\n"));

    panel->NativeModeIndex = VIA_PANEL_INVALID;
    if (strlen(name) < 10) {
        length = sizeof(ViaPanelNativeModes) / sizeof(ViaPanelModeRec);

        for (index = 0; index < length; index++) {
            sprintf(aux, "%dx%d", ViaPanelNativeModes[index].Width,
                    ViaPanelNativeModes[index].Height);
            if (!xf86NameCmp(name, aux)) {
                panel->NativeModeIndex = index;
                panel->NativeMode->Width = ViaPanelNativeModes[index].Width;
                panel->NativeMode->Height = ViaPanelNativeModes[index].Height;
                break;
            }
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "%s is not a valid panel size.\n", name);
    }
}

void
via_lvds_init(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	VIAPtr pVia = VIAPTR(pScrn);
	VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
	xf86OutputPtr output = NULL;

	if (pBIOSInfo->ForcePanel) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Enabling panel from config.\n");
		output = xf86OutputCreate(pScrn, &via_lvds_funcs, "LVDS-0");
	} else if (pVia->Id && (pVia->Id->Outputs & VIA_DEVICE_LCD)) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Enabling panel from PCI-subsystem ID information.\n");
		output = xf86OutputCreate(pScrn, &via_lvds_funcs, "LVDS-0");
	}

	if (output)  {
        output->possible_crtcs = 0x2;
        output->possible_clones = 0;
        output->interlaceAllowed = FALSE;
        pBIOSInfo->lvds = output;
    }
}
