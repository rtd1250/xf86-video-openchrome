/*
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "via_driver.h"
#include "via_vt1632.h"

static Bool
xf86I2CMaskByte(I2CDevPtr d, I2CByte subaddr, I2CByte value, I2CByte mask)
{
	I2CByte tmp;
	Bool ret;

	ret = xf86I2CReadByte(d, subaddr, &tmp);
	if (!ret)
		return FALSE;

	tmp &= ~mask;
	tmp |= (value & mask);

	return xf86I2CWriteByte(d, subaddr, tmp);
}

static void
via_vt1632_dump_registers(ScrnInfoPtr pScrn, I2CDevPtr pDev)
{
	int i;
	CARD8 tmp;

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: dumping registers:\n"));
	for (i = 0; i <= 0x0f; i++) {
		xf86I2CReadByte(pDev, i, &tmp);
		DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: 0x%02x: 0x%02x\n", i, tmp));
	}
}


void
via_vt1632_power(xf86OutputPtr output, BOOL on)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;


	if (on == TRUE) {
		DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: power on\n"));
		xf86I2CMaskByte(Private->VT1632I2CDev, 0x08, 0x01, 0x01);
	} else {
		DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: power off\n"));
		xf86I2CMaskByte(Private->VT1632I2CDev, 0x08, 0x00, 0x01);
	}
}

void
via_vt1632_save(xf86OutputPtr output)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"VT1632: saving state of registers 0x08, 0x09, 0x0A and 0x0C\n"));

	xf86I2CReadByte(Private->VT1632I2CDev, 0x08, &Private->Register08);
	xf86I2CReadByte(Private->VT1632I2CDev, 0x09, &Private->Register09);
	xf86I2CReadByte(Private->VT1632I2CDev, 0x0A, &Private->Register0A);
	xf86I2CReadByte(Private->VT1632I2CDev, 0x0C, &Private->Register0C);
}

void
via_vt1632_restore(xf86OutputPtr output)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"VT1632: restoring register 0x08, 0x09, 0x0A and 0x0C\n"));
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x08, Private->Register08);
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x09, Private->Register09);
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x0A, Private->Register0A);
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x0C, Private->Register0C);
}

int
via_vt1632_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;

	if (pMode->Clock < Private->DotclockMin)
		return MODE_CLOCK_LOW;

	if (pMode->Clock > Private->DotclockMax)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

void
via_vt1632_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;

	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"VT1632: enabling DVI encoder\n"));
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x0C, 0x89);
	xf86I2CWriteByte(Private->VT1632I2CDev, 0x08,
		VIA_VT1632_VEN | VIA_VT1632_HEN | VIA_VT1632_EDGE |
		VIA_VT1632_PDB);
	via_vt1632_dump_registers(pScrn, Private->VT1632I2CDev);
}

xf86OutputStatus
via_vt1632_detect(xf86OutputPtr output)
{
	struct ViaVT1632PrivateData * Private = output->driver_private;
	xf86OutputStatus status = XF86OutputStatusDisconnected;
	ScrnInfoPtr pScrn = output->scrn;
	CARD8 tmp;

	xf86I2CReadByte(Private->VT1632I2CDev, 0x09, &tmp);
	if (tmp && 0x02) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: DVI is connected\n");
		status = XF86OutputStatusConnected;
	}

	return status;
}

BOOL
via_vt1632_probe(ScrnInfoPtr pScrn, I2CDevPtr pDev) {
	CARD8 buf = 0;
	CARD16 VendorID = 0;
	CARD16 DeviceID = 0;

	xf86I2CReadByte(pDev, 0, &buf);
	VendorID = buf;
	xf86I2CReadByte(pDev, 1, &buf);
	VendorID |= buf << 8;

	xf86I2CReadByte(pDev, 2, &buf);
	DeviceID = buf;
	xf86I2CReadByte(pDev, 3, &buf);
	DeviceID |= buf << 8;

	if (VendorID != 0x1106 || DeviceID != 0x3192) {
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632 DVI transmitter detected\n");

	return TRUE;
}

struct ViaVT1632PrivateData *
via_vt1632_init(ScrnInfoPtr pScrn, I2CDevPtr pDev)
{
	VIAPtr pVia = VIAPTR(pScrn);
	struct ViaVT1632PrivateData * Private = NULL;
	CARD8 buf = 0;

	Private = xnfcalloc(1, sizeof(struct ViaVT1632PrivateData));
	if (!Private) {
		return NULL;
	}
	Private->VT1632I2CDev = pDev;

	xf86I2CReadByte(pDev, 0x06, &buf);
	Private->DotclockMin = buf * 1000;

	xf86I2CReadByte(pDev, 0x07, &buf);
	Private->DotclockMax = (buf + 65) * 1000;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VT1632: dotclock range %d-%dMHz\n",
			Private->DotclockMin / 1000,
			Private->DotclockMax / 1000);

	via_vt1632_dump_registers(pScrn, pDev);

	return Private;
}
