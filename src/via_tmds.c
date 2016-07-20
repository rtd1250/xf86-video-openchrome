/*
 * Copyright 2016 Kevin Brace
 * Copyright 2015-2016 The OpenChrome Project
 *                     [http://www.freedesktop.org/wiki/Openchrome]
 * Copyright 2014 SHS SERVICES GmbH
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
 * via_tmds.c
 *
 * Handles initialization of TMDS (DVI) related resources and 
 * controls the integrated TMDS transmitter found in CX700 and 
 * later VIA Technologies chipsets.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include "via_driver.h"
#include "via_vt1632.h"


void
viaTMDSPower(ScrnInfoPtr pScrn, Bool On)
{

    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaTMDSPower.\n"));

    if (On) {
        /* Power on TMDS */
        ViaCrtcMask(hwp, 0xD2, 0x00, 0x08);
    } else {
        /* Power off TMDS */
        ViaCrtcMask(hwp, 0xD2, 0x08, 0x08);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                "Integrated TMDS (DVI) Power: %s\n",
                On ? "On" : "Off");

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaTMDSPower.\n"));
}

static void
via_dvi_create_resources(xf86OutputPtr output)
{
}

#ifdef RANDR_12_INTERFACE
static Bool
via_dvi_set_property(xf86OutputPtr output, Atom property,
                     RRPropertyValuePtr value)
{
    return TRUE;
}

static Bool
via_dvi_get_property(xf86OutputPtr output, Atom property)
{
    return FALSE;
}
#endif

static void
via_dvi_dpms(xf86OutputPtr output, int mode)
{
    ScrnInfoPtr pScrn = output->scrn;

    switch (mode) {
    case DPMSModeOn:
        via_vt1632_power(output, TRUE);
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
        via_vt1632_power(output, FALSE);
        break;
    default:
        break;
    }
}

static void
via_dvi_save(xf86OutputPtr output)
{
    via_vt1632_save(output);
}

static void
via_dvi_restore(xf86OutputPtr output)
{
    via_vt1632_restore(output);
}

static int
via_dvi_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    return via_vt1632_mode_valid(output, pMode);
}

static Bool
via_dvi_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
                   DisplayModePtr adjusted_mode)
{
    return TRUE;
}

static void
via_dvi_prepare(xf86OutputPtr output)
{
}

static void
via_dvi_commit(xf86OutputPtr output)
{
}

static void
via_dvi_mode_set(xf86OutputPtr output, DisplayModePtr mode,
                 DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    via_vt1632_mode_set(output, mode, adjusted_mode);
}

static xf86OutputStatus
via_dvi_detect(xf86OutputPtr output)
{
    xf86OutputStatus status = XF86OutputStatusDisconnected;
    ScrnInfoPtr pScrn = output->scrn;
    VIAPtr pVia = VIAPTR(pScrn);
    ViaVT1632Ptr Private = output->driver_private;
    xf86MonPtr mon;

    /* Check for the DVI presence via VT1632A first before accessing
     * I2C bus. */
    status = via_vt1632_detect(output);
    if (status == XF86OutputStatusConnected) {

        /* Since DVI presence was established, access the I2C bus
         * assigned to DVI. */
        mon = xf86OutputGetEDID(output, Private->VT1632I2CDev->pI2CBus);

        /* Is the interface type digital? */
        if (mon && DIGITAL(mon->features.input_type)) {
            xf86OutputSetEDID(output, mon);
        } else {
            status = XF86OutputStatusDisconnected;
        }
    }

    return status;
}

static void
via_dvi_destroy(xf86OutputPtr output)
{
}

static const xf86OutputFuncsRec via_dvi_funcs = {
    .create_resources   = via_dvi_create_resources,
#ifdef RANDR_12_INTERFACE
    .set_property       = via_dvi_set_property,
#endif
#ifdef RANDR_13_INTERFACE
    .get_property       = via_dvi_get_property,
#endif
    .dpms               = via_dvi_dpms,
    .save               = via_dvi_save,
    .restore            = via_dvi_restore,
    .mode_valid         = via_dvi_mode_valid,
    .mode_fixup         = via_dvi_mode_fixup,
    .prepare            = via_dvi_prepare,
    .commit             = via_dvi_commit,
    .mode_set           = via_dvi_mode_set,
    .detect             = via_dvi_detect,
    .get_modes          = xf86OutputGetEDIDModes,
    .destroy            = via_dvi_destroy,
};

void
via_dvi_init(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    xf86OutputPtr output = NULL;
    ViaVT1632Ptr private_data = NULL;
    I2CBusPtr pBus = NULL;
    I2CDevPtr pDev = NULL;
    I2CSlaveAddr addr = 0x10;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered via_dvi_init.\n"));

    if (!pVia->pI2CBus2 || !pVia->pI2CBus3) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "I2C Bus 2 or I2C Bus 3 does not exist.\n");
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                    "Exiting via_dvi_init.\n"));
        return;
    }

    if (xf86I2CProbeAddress(pVia->pI2CBus3, addr)) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                            "Will probe I2C Bus 3 for a possible "
                            "external TMDS transmitter.\n"));
        pBus = pVia->pI2CBus3;
    } else if (xf86I2CProbeAddress(pVia->pI2CBus2, addr)) {
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                            "Will probe I2C Bus 2 for a possible "
                            "external TMDS transmitter.\n"));
        pBus = pVia->pI2CBus2;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                    "Did not find a possible external TMDS transmitter "
                    "on I2C Bus 2 or I2C Bus 3.\n");
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                            "Exiting via_dvi_init.\n"));
        return;
    }

    pDev = xf86CreateI2CDevRec();
    if (!pDev) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Failed to create an I2C bus structure.\n");
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                            "Exiting via_dvi_init.\n"));
        return;
    }

    pDev->DevName = "VT1632A";
    pDev->SlaveAddr = addr;
    pDev->pI2CBus = pBus;
    if (!xf86I2CDevInit(pDev)) {
        xf86DestroyI2CDevRec(pDev, TRUE);
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Failed to initialize a device on I2C bus.\n");
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                            "Exiting via_dvi_init.\n"));
        return;
    }

    if (!via_vt1632_probe(pScrn, pDev)) {
        xf86DestroyI2CDevRec(pDev, TRUE);
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                            "Exiting via_dvi_init.\n"));
        return;
    }

    private_data = via_vt1632_init(pScrn, pDev);
    if (!private_data) {
        xf86DestroyI2CDevRec(pDev, TRUE);
        DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                            "Exiting via_dvi_init.\n"));
        return;
    }

    output = xf86OutputCreate(pScrn, &via_dvi_funcs, "DVI-1");
    if (output) {
        output->driver_private = private_data;
        output->possible_crtcs = 0x2;
        output->possible_clones = 0;
        output->interlaceAllowed = FALSE;
        output->doubleScanAllowed = FALSE;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting via_dvi_init.\n"));
}

#ifdef HAVE_DEBUG
/*
 * Returns:
 *   Bit[7] 2nd Path
 *   Bit[6] 1/0 MHS Enable/Disable
 *   Bit[5] 0 = Bypass Callback, 1 = Enable Callback
 *   Bit[4] 0 = Hot-Key Sequence Control (OEM Specific)
 *   Bit[3] LCD
 *   Bit[2] TV
 *   Bit[1] CRT
 *   Bit[0] DVI
 */
/*
static CARD8
VIAGetActiveDisplay(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 tmp;

    tmp = (hwp->readCrtc(hwp, 0x3E) >> 4);
    tmp |= ((hwp->readCrtc(hwp, 0x3B) & 0x18) << 3);

    return tmp;
}
*/
#endif /* HAVE_DEBUG */
