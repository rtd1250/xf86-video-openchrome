/*
 * Copyright 2005-2007 The Openchrome Project  [openchrome.org]
 * Copyright 2004-2006 Luc Verhaegen.
 * Copyright 2004-2005 The Unichrome Project  [unichrome.sf.net]
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shadowfb.h"

#include "globals.h"
#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif

#include "svnversion.h"

#include "via_driver.h"
#include "via_video.h"
#include "via.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86RAC.h"
#endif
#include "xf86Crtc.h"

#ifdef XF86DRI
#include "dri.h"
#endif
#include "via_id.h"

/* RandR support */
#include "xf86RandR12.h"

typedef struct
{
	int major;
	int minor;
	int patchlevel;
} ViaDRMVersion;

static const ViaDRMVersion drmExpected = { 1, 3, 0 };
static const ViaDRMVersion drmCompat = { 3, 1, 0 };

/* Prototypes. */
static void VIAIdentify(int flags);

#ifdef XSERVER_LIBPCIACCESS
struct pci_device *
via_pci_device(const struct pci_slot_match *bridge_match)
{
    struct pci_device_iterator *slot_iterator;
    struct pci_device *bridge;

    slot_iterator = pci_slot_match_iterator_create(bridge_match);
    bridge = pci_device_next(slot_iterator);
    pci_iterator_destroy(slot_iterator);
    return bridge;
}

struct pci_device *
via_host_bridge(void)
{
    static const struct pci_slot_match bridge_match = {
        0, 0, 0, 0, 0
    };
    return via_pci_device(&bridge_match);
}

struct pci_device *
viaPciDeviceVga(void)
{
    static const struct pci_slot_match bridge_match = {
        0, 0, 0, 3, 0
    };
    return via_pci_device(&bridge_match);
}

static Bool via_pci_probe(DriverPtr drv, int entity_num,
                          struct pci_device *dev, intptr_t match_data);
#else /* !XSERVER_LIBPCIACCESS */
static Bool VIAProbe(DriverPtr drv, int flags);
#endif

static Bool VIASetupDefaultOptions(ScrnInfoPtr pScrn);
static Bool VIAPreInit(ScrnInfoPtr pScrn, int flags);
static Bool VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
                          char **argv);
static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static const OptionInfoRec *VIAAvailableOptions(int chipid, int busid);

Bool UMSPreInit(ScrnInfoPtr pScrn);
Bool UMSCrtcInit(ScrnInfoPtr pScrn);
void UMSAccelSetup(ScrnInfoPtr pScrn);

#ifdef XSERVER_LIBPCIACCESS

#define VIA_DEVICE_MATCH(d,i) \
    { 0x1106, (d), PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, (i) }

static const struct pci_id_match via_device_match[] = {
   VIA_DEVICE_MATCH (PCI_CHIP_VT3204, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3259, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_CLE3122, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3205, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3314, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3336, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3364, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3324, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3327, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3353, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3409, 0 ),
   VIA_DEVICE_MATCH (PCI_CHIP_VT3410, 0 ),
    { 0, 0, 0 },
};

#endif /* XSERVER_LIBPCIACCESS */

_X_EXPORT DriverRec VIA = {
    VIA_VERSION,
    DRIVER_NAME,
    VIAIdentify,
#ifdef XSERVER_LIBPCIACCESS
    NULL,
#else
    VIAProbe,
#endif
    VIAAvailableOptions,
    NULL,
    0,
    NULL,
#ifdef XSERVER_LIBPCIACCESS
    via_device_match,
    via_pci_probe
#endif
};

/* Supported chipsets */
static SymTabRec VIAChipsets[] = {
    {VIA_CLE266,   "CLE266"},
    {VIA_KM400,    "KM400/KN400"},
    {VIA_K8M800,   "K8M800/K8N800"},
    {VIA_PM800,    "PM800/PM880/CN400"},
    {VIA_VM800,    "VM800/P4M800Pro/VN800/CN700"},
    {VIA_CX700,    "CX700/VX700"},
    {VIA_K8M890,   "K8M890/K8N890"},
    {VIA_P4M890,   "P4M890"},
    {VIA_P4M900,   "P4M900/VN896/CN896"},
    {VIA_VX800,    "VX800/VX820"},
    {VIA_VX855,    "VX855/VX875"},
    {VIA_VX900,    "VX900"},
    {-1,            NULL }
};

/* Mapping a PCI device ID to a chipset family identifier. */
static PciChipsets VIAPciChipsets[] = {
    {VIA_CLE266,   PCI_CHIP_CLE3122,   VIA_RES_SHARED},
    {VIA_KM400,    PCI_CHIP_VT3205,    VIA_RES_SHARED},
    {VIA_K8M800,   PCI_CHIP_VT3204,    VIA_RES_SHARED},
    {VIA_PM800,    PCI_CHIP_VT3259,    VIA_RES_SHARED},
    {VIA_VM800,    PCI_CHIP_VT3314,    VIA_RES_SHARED},
    {VIA_CX700,    PCI_CHIP_VT3324,    VIA_RES_SHARED},
    {VIA_K8M890,   PCI_CHIP_VT3336,    VIA_RES_SHARED},
    {VIA_P4M890,   PCI_CHIP_VT3327,    VIA_RES_SHARED},
    {VIA_P4M900,   PCI_CHIP_VT3364,    VIA_RES_SHARED},
    {VIA_VX800,    PCI_CHIP_VT3353,    VIA_RES_SHARED},
    {VIA_VX855,    PCI_CHIP_VT3409,    VIA_RES_SHARED},
    {VIA_VX900,    PCI_CHIP_VT3410,    VIA_RES_SHARED},
    {-1,           -1,                 VIA_RES_UNDEF}
};

typedef enum
{
#ifdef HAVE_DEBUG
    OPTION_PRINTVGAREGS,
    OPTION_PRINTTVREGS,
    OPTION_I2CSCAN,
#endif
    OPTION_VBEMODES,
    OPTION_NOACCEL,
    OPTION_ACCELMETHOD,
    OPTION_EXA_NOCOMPOSITE,
    OPTION_EXA_SCRATCH_SIZE,
    OPTION_SWCURSOR,
    OPTION_SHADOW_FB,
    OPTION_ROTATION_TYPE,
    OPTION_ROTATE,
    OPTION_VIDEORAM,
    OPTION_ACTIVEDEVICE,
    OPTION_I2CDEVICES,
    OPTION_BUSWIDTH,
    OPTION_CENTER,
    OPTION_PANELSIZE,
    OPTION_FORCEPANEL,
    OPTION_TVDOTCRAWL,
    OPTION_TVTYPE,
    OPTION_TVOUTPUT,
    OPTION_TVDIPORT,
    OPTION_DISABLEVQ,
    OPTION_DISABLEIRQ,
    OPTION_TVDEFLICKER,
    OPTION_AGP_DMA,
    OPTION_2D_DMA,
    OPTION_XV_DMA,
    OPTION_VBE_SAVERESTORE,
    OPTION_MAX_DRIMEM,
    OPTION_AGPMEM,
    OPTION_DISABLE_XV_BW_CHECK,
    OPTION_MODE_SWITCH_METHOD
} VIAOpts;

static OptionInfoRec VIAOptions[] = {
#ifdef HAVE_DEBUG /* Don't document these three. */
    {OPTION_PRINTVGAREGS,        "PrintVGARegs",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_PRINTTVREGS,         "PrintTVRegs",      OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_I2CSCAN,             "I2CScan",          OPTV_BOOLEAN, {0}, FALSE},
#endif
    {OPTION_VBEMODES,            "VBEModes",         OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_NOACCEL,             "NoAccel",          OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_ACCELMETHOD,         "AccelMethod",      OPTV_STRING,  {0}, FALSE},
    {OPTION_EXA_NOCOMPOSITE,     "ExaNoComposite",   OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_EXA_SCRATCH_SIZE,    "ExaScratchSize",   OPTV_INTEGER, {0}, FALSE},
    {OPTION_SWCURSOR,            "SWCursor",         OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_SHADOW_FB,           "ShadowFB",         OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_ROTATION_TYPE,       "RotationType",     OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_ROTATE,              "Rotate",           OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_VIDEORAM,            "VideoRAM",         OPTV_INTEGER, {0}, FALSE},
    {OPTION_ACTIVEDEVICE,        "ActiveDevice",     OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_BUSWIDTH,            "BusWidth",         OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_CENTER,              "Center",           OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_PANELSIZE,           "PanelSize",        OPTV_ANYSTR,  {0}, FALSE},
    /* Forcing use of panel is a last resort - don't document this one. */
    {OPTION_FORCEPANEL,          "ForcePanel",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_TVDOTCRAWL,          "TVDotCrawl",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_TVDEFLICKER,         "TVDeflicker",      OPTV_INTEGER, {0}, FALSE},
    {OPTION_TVTYPE,              "TVType",           OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVOUTPUT,            "TVOutput",         OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_TVDIPORT,            "TVPort",           OPTV_ANYSTR,  {0}, FALSE}, 
    {OPTION_DISABLEVQ,           "DisableVQ",        OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_DISABLEIRQ,          "DisableIRQ",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_AGP_DMA,             "EnableAGPDMA",     OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_2D_DMA,              "NoAGPFor2D",       OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_XV_DMA,              "NoXVDMA",          OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_VBE_SAVERESTORE,     "VbeSaveRestore",   OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_DISABLE_XV_BW_CHECK, "DisableXvBWCheck", OPTV_BOOLEAN, {0}, FALSE},
    {OPTION_MODE_SWITCH_METHOD,  "ModeSwitchMethod", OPTV_ANYSTR,  {0}, FALSE},
    {OPTION_MAX_DRIMEM,          "MaxDRIMem",        OPTV_INTEGER, {0}, FALSE},
    {OPTION_AGPMEM,              "AGPMem",           OPTV_INTEGER, {0}, FALSE},
    {OPTION_I2CDEVICES,          "I2CDevices",       OPTV_ANYSTR,  {0}, FALSE},
    {-1,                         NULL,               OPTV_NONE,    {0}, FALSE}
};

#ifdef XFree86LOADER
static MODULESETUPPROTO(VIASetup);

static XF86ModuleVersionInfo VIAVersRec = {
    "openchrome",
    "http://openchrome.org/",
    MODINFOSTRING1,
    MODINFOSTRING2,
#ifdef XORG_VERSION_CURRENT
    XORG_VERSION_CURRENT,
#else
    XF86_VERSION_CURRENT,
#endif
    VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData openchromeModuleData = { &VIAVersRec, VIASetup, NULL };

static pointer
VIASetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    /* Only be loaded once */
    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&VIA, module,
#ifdef XSERVER_LIBPCIACCESS
                     HaveDriverFuncs
#else
                     0
#endif
                     );

        return (pointer) 1;
    } else {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;

        return NULL;
    }
} /* VIASetup */

#endif /* XFree86LOADER */

static const OptionInfoRec *
VIAAvailableOptions(int chipid, int busid)
{
    return VIAOptions;
}

static Bool
VIASwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    return xf86SetSingleMode(pScrn, mode, RR_Rotate_0);
}

static void
VIAAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int i;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAAdjustFrame %dx%d\n", x, y));

    for (i = 0; i < xf86_config->num_crtc; i++) {
        xf86CrtcPtr crtc = xf86_config->crtc[i];

        xf86CrtcSetOrigin(crtc, x, y);
    }
}

static Bool
VIAEnterVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	VIAPtr pVia = VIAPTR(pScrn);
	Bool ret;
	int i;

	DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIAEnterVT\n"));

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (crtc->funcs->save)
			crtc->funcs->save(crtc);
	}

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->funcs->save)
			output->funcs->save(output);
	}

	if (!xf86SetDesiredModes(pScrn))
		return FALSE;

	xf86SaveScreen(pScrn->pScreen, SCREEN_SAVER_ON);
	xf86_reload_cursors(pScrn->pScreen);

	/* Restore video status. */
	if (!pVia->IsSecondary)
		viaRestoreVideo(pScrn);

#ifdef XF86DRI
	if (pVia->directRenderingType) {
		kickVblank(pScrn);
		VIADRIRingBufferInit(pScrn);
		viaDRIOffscreenRestore(pScrn);
	}
#endif
	if (pVia->NoAccel) {
		memset(pVia->FBBase, 0x00, pVia->Bpl * pScrn->virtualY);
	} else {
		viaAccelFillRect(pScrn, 0, 0, pScrn->displayWidth, pScrn->virtualY,
						0x00000000);
		viaAccelSyncMarker(pScrn);
	}

#ifdef XF86DRI
	if (pVia->directRenderingType)
		DRIUnlock(screenInfo.screens[scrnIndex]);
#endif
	return ret;
}

static void
VIALeaveVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	VIAPtr pVia = VIAPTR(pScrn);
	int i;

	DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIALeaveVT\n"));

#ifdef XF86DRI
	if (pVia->directRenderingType) {
		volatile drm_via_sarea_t *saPriv = (drm_via_sarea_t *) DRIGetSAREAPrivate(pScrn->pScreen);

		DRILock(screenInfo.screens[scrnIndex], 0);
		saPriv->ctxOwner = ~0;
	}
#endif

	viaAccelSync(pScrn);

#ifdef XF86DRI
	if (pVia->directRenderingType) {
		VIADRIRingBufferCleanup(pScrn);
		viaDRIOffscreenSave(pScrn);
	}
#endif

	if (pVia->VQEnable)
		viaDisableVQ(pScrn);

	/* Save video status and turn off all video activities. */
	if (!pVia->IsSecondary)
		viaSaveVideo(pScrn);

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->funcs->restore)
			output->funcs->restore(output);
	}

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (crtc->funcs->restore)
			crtc->funcs->restore(crtc);
	}

	VIAUnmapMem(pScrn);

	pScrn->vtSema = FALSE;
}

static void
VIAIdentify(int flags)
{
    xf86PrintChipsets("OPENCHROME", "Driver for VIA Chrome chipsets",
                      VIAChipsets);
}

#ifdef XSERVER_LIBPCIACCESS
static Bool
via_pci_probe(DriverPtr driver, int entity_num,
              struct pci_device *device, intptr_t match_data)
{
    ScrnInfoPtr scrn = NULL;
    EntityInfoPtr entity;

    scrn = xf86ConfigPciEntity(scrn, 0, entity_num, VIAPciChipsets,
                               NULL, NULL, NULL, NULL, NULL);

    if (scrn != NULL) {
        scrn->driverVersion = VIA_VERSION;
        scrn->driverName = DRIVER_NAME;
        scrn->name = "CHROME";
        scrn->Probe = NULL;

        entity = xf86GetEntityInfo(entity_num);

        scrn->PreInit = VIAPreInit;
        scrn->ScreenInit = VIAScreenInit;
        scrn->SwitchMode = VIASwitchMode;
        scrn->AdjustFrame = VIAAdjustFrame;
		scrn->EnterVT = VIAEnterVT;
		scrn->LeaveVT = VIALeaveVT;
		UMSInit(scrn);

        xf86Msg(X_NOTICE,
                "VIA Technologies does not support this driver in any way.\n");
        xf86Msg(X_NOTICE,
                "For support, please refer to http://www.openchrome.org/.\n");
#ifdef BUILDCOMMENT
        xf86Msg(X_NOTICE, BUILDCOMMENT"\n");
#endif
    }
    return scrn != NULL;
}
#else /* !XSERVER_LIBPCIACCESS */
static Bool
VIAProbe(DriverPtr drv, int flags)
{
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    int i;

    /* sanity checks */
    if ((numDevSections = xf86MatchDevice(DRIVER_NAME, &devSections)) <= 0)
        return FALSE;

    if (xf86GetPciVideoInfo() == NULL)
        return FALSE;

    numUsed = xf86MatchPciInstances(DRIVER_NAME,
                                    PCI_VIA_VENDOR_ID,
                                    VIAChipsets,
                                    VIAPciChipsets,
                                    devSections,
                                    numDevSections,
                                    drv,
                                    &usedChips);
    free(devSections);

    if (numUsed <= 0)
        return FALSE;

    xf86Msg(X_NOTICE,
            "VIA Technologies does not support this driver in any way.\n");
    xf86Msg(X_NOTICE, "For support, please refer to http://openchrome.org/.\n");

#ifdef BUILDCOMMENT
    xf86Msg(X_NOTICE, BUILDCOMMENT"\n");
#endif

    if (flags & PROBE_DETECT) {
        foundScreen = TRUE;
    } else {
        for (i = 0; i < numUsed; i++) {
            ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);
            EntityInfoPtr pEnt;

            if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
                                             VIAPciChipsets, 0, 0, 0, 0, 0))) {
                pScrn->driverVersion = VIA_VERSION;
                pScrn->driverName = DRIVER_NAME;
                pScrn->name = "CHROME";
                pScrn->Probe = VIAProbe;
                pScrn->PreInit = VIAPreInit;
                pScrn->ScreenInit = VIAScreenInit;
                foundScreen = TRUE;
				UMSFreeScreen(pScrn);
            }
#if 0
            xf86ConfigActivePciEntity(pScrn,
                                      usedChips[i],
                                      VIAPciChipsets,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
#endif
            pEnt = xf86GetEntityInfo(usedChips[i]);

            /* CLE266 supports dual-head; mark the entity as sharable. */
            if (pEnt->chipset == VIA_CLE266 || pEnt->chipset == VIA_KM400) {
                static int instance = 0;
                DevUnion *pPriv;

                xf86SetEntitySharable(usedChips[i]);
                xf86SetEntityInstanceForScreen(pScrn,
                                               pScrn->entityList[0], instance);

                if (gVIAEntityIndex < 0) {
                    gVIAEntityIndex = xf86AllocateEntityPrivateIndex();
                    pPriv = xf86GetEntityPrivate(pScrn->entityList[0],
                                                 gVIAEntityIndex);

                    if (!pPriv->ptr) {
                        VIAEntPtr pVIAEnt;

                        pPriv->ptr = xnfcalloc(sizeof(VIAEntRec), 1);
                        pVIAEnt = pPriv->ptr;
                        pVIAEnt->IsDRIEnabled = FALSE;
                        pVIAEnt->BypassSecondary = FALSE;
                        pVIAEnt->HasSecondary = FALSE;
                        pVIAEnt->IsSecondaryRestored = FALSE;
                    }
                }
                instance++;
            }
            free(pEnt);
        }
    }

    free(usedChips);

    return foundScreen;

} /* VIAProbe */
#endif /* !XSERVER_LIBPCIACCESS */

static int
LookupChipSet(PciChipsets *pset, int chipSet)
{
    while (pset->numChipset >= 0) {
        if (pset->numChipset == chipSet)
            return pset->PCIid;
        pset++;
    }
    return -1;
}


static int
LookupChipID(PciChipsets *pset, int ChipID)
{
    /* Is there a function to do this for me? */
    while (pset->numChipset >= 0) {
        if (pset->PCIid == ChipID)
            return pset->numChipset;

        pset++;
    }

    return -1;
}

static Bool
VIASetupDefaultOptions(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIASetupDefaultOptions - Setting up default chipset options.\n"));

    pVia->shadowFB = FALSE;
    pVia->NoAccel = FALSE;
    pVia->noComposite = FALSE;
    pVia->useEXA = TRUE;
    pVia->exaScratchSize = VIA_SCRATCH_SIZE / 1024;
    pVia->hwcursor = TRUE;
    pVia->VQEnable = TRUE;
    pVia->DRIIrqEnable = TRUE;
    pVia->agpEnable = TRUE;
    pVia->dma2d = TRUE;
    pVia->dmaXV = TRUE;
    pVia->useVBEModes = FALSE;
    pVia->vbeSR = FALSE;
#ifdef HAVE_DEBUG
    pVia->disableXvBWCheck = FALSE;
#endif
    pVia->maxDriSize = 0;
    pVia->agpMem = AGP_SIZE / 1024;
    pVia->ActiveDevice = 0x00;
    pVia->I2CDevices = 0x00;
    pVia->VideoEngine = VIDEO_ENGINE_CLE;
#ifdef HAVE_DEBUG
    pVia->PrintVGARegs = FALSE;
#endif

    /* Disable vertical interpolation because the size of */
    /* line buffer (limited to 800) is too small to do interpolation. */
    pVia->swov.maxWInterp = 800;
    pVia->swov.maxHInterp = 600;
    pVia->useLegacyVBE = TRUE;

    pVia->UseLegacyModeSwitch = FALSE;
    pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP1;

    switch (pVia->Chipset) {
        case VIA_CLE266:
            pVia->UseLegacyModeSwitch = TRUE;
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0;
			/* FIXME Mono HW Cursors not working */
			pVia->hwcursor = FALSE;
            break;
        case VIA_KM400:
            /* IRQ is not broken on KM400A, but testing (pVia->ChipRev < 0x80)
             * is not enough to make sure we have an older, broken KM400. */
            pVia->DRIIrqEnable = FALSE;
			/* FIXME Mono HW Cursors not working */
			pVia->hwcursor = FALSE;
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0;
            break;
        case VIA_K8M800:
            pVia->DRIIrqEnable = FALSE;
            break;
        case VIA_PM800:
            /* Use new mode switch to resolve many resolution and display bugs (switch to console) */
            /* FIXME The video playing (XV) is not working correctly after turn on new mode switch */
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            break;
        case VIA_VM800:
            /* New mode switch resolve bug with gamma set #282 */
            /* and with Xv after hibernate #240                */
            break;
        case VIA_CX700:
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            pVia->swov.maxWInterp = 1920;
            pVia->swov.maxHInterp = 1080;
            break;
        case VIA_K8M890:
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            pVia->agpEnable = FALSE;
            pVia->dmaXV = FALSE;
            break;
        case VIA_P4M890:
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            pVia->dmaXV = FALSE;
            break;
        case VIA_P4M900:
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            pVia->agpEnable = FALSE;
            pVia->useLegacyVBE = FALSE;
            /* FIXME: this needs to be tested */
            pVia->dmaXV = FALSE;
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0;
            break;

        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            pVia->agpEnable = FALSE;
            pVia->dmaXV = FALSE;
            break;
    }

    return TRUE;
}

static Bool
VIAPreInit(ScrnInfoPtr pScrn, int flags)
{
    EntityInfoPtr pEnt;
    VIAPtr pVia;
    VIABIOSInfoPtr pBIOSInfo;
    MessageType from = X_DEFAULT;
    char *s = NULL;
#ifndef USE_FB
    char *mod = NULL;
#endif

#ifdef XSERVER_LIBPCIACCESS
    struct pci_device *bridge = via_host_bridge();
    uint8_t rev = 0 ;
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAPreInit\n"));

    if (pScrn->numEntities > 1)
        return FALSE;

    if (flags & PROBE_DETECT)
        return FALSE;

    if (!VIAGetRec(pScrn)) {
        return FALSE;
    }

    pVia = VIAPTR(pScrn);
    pVia->IsSecondary = FALSE;
    pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
#ifndef XSERVER_LIBPCIACCESS
    if (pEnt->resources) {
        free(pEnt);
        VIAFreeRec(pScrn);
        return FALSE;
    }
#endif

    pVia->EntityIndex = pEnt->index;

    if (xf86IsEntityShared(pScrn->entityList[0])) {
        if (xf86IsPrimInitDone(pScrn->entityList[0])) {
            DevUnion *pPriv;
            VIAEntPtr pVIAEnt;
            VIAPtr pVia1;

            pVia->IsSecondary = TRUE;
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gVIAEntityIndex);
            pVIAEnt = pPriv->ptr;
            if (pVIAEnt->BypassSecondary) {
                free(pEnt);
                VIAFreeRec(pScrn);
                return FALSE;
            }
            pVIAEnt->pSecondaryScrn = pScrn;
            pVIAEnt->HasSecondary = TRUE;
            pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
            pVia1->HasSecondary = TRUE;
            pVia->sharedData = pVia1->sharedData;
        } else {
            DevUnion *pPriv;
            VIAEntPtr pVIAEnt;

            xf86SetPrimInitDone(pScrn->entityList[0]);
            pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gVIAEntityIndex);
            pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec), 1);
            pVIAEnt = pPriv->ptr;
            pVIAEnt->pPrimaryScrn = pScrn;
            pVIAEnt->IsDRIEnabled = FALSE;
            pVIAEnt->BypassSecondary = FALSE;
            pVIAEnt->HasSecondary = FALSE;
            pVIAEnt->RestorePrimary = FALSE;
            pVIAEnt->IsSecondaryRestored = FALSE;
        }
    } else {
        pVia->sharedData = xnfcalloc(sizeof(ViaSharedRec), 1);
    }

    pVia->PciInfo = xf86GetPciInfoForEntity(pEnt->index);
#ifndef XSERVER_LIBPCIACCESS
    xf86RegisterResources(pEnt->index, NULL, ResNone);
#endif

#if 0
    xf86SetOperatingState(RES_SHARED_VGA, pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMemShared, pEnt->index, ResDisableOpr);
#endif

    if (pEnt->device->chipset && *pEnt->device->chipset) {
        from = X_CONFIG;
        pScrn->chipset = pEnt->device->chipset;
        pVia->Chipset = xf86StringToToken(VIAChipsets, pScrn->chipset);
        pVia->ChipId = LookupChipSet(VIAPciChipsets, pVia->Chipset);
    } else if (pEnt->device->chipID >= 0) {
        from = X_CONFIG;
        pVia->ChipId = pEnt->device->chipID;
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets, pVia->Chipset);
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
                   pEnt->device->chipID);
    } else {
        from = X_PROBED;
        pVia->ChipId = DEVICE_ID(pVia->PciInfo);
        pVia->Chipset = LookupChipID(VIAPciChipsets, pVia->ChipId);
        pScrn->chipset = (char *)xf86TokenToString(VIAChipsets, pVia->Chipset);
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: %s\n", pScrn->chipset);

    if (pEnt->device->chipRev >= 0) {
        pVia->ChipRev = pEnt->device->chipRev;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
                   pVia->ChipRev);
    } else {
        /* Read PCI bus 0, dev 0, function 0, index 0xF6 to get chip revision */
#ifdef XSERVER_LIBPCIACCESS
	pci_device_cfg_read_u8(bridge, &rev, 0xF6);
	pVia->ChipRev = rev ;
#else
        pVia->ChipRev = pciReadByte(pciTag(0, 0, 0), 0xF6);
#endif
    }
    free(pEnt);

    if (!UMSPreInit(pScrn)) {
        VIAFreeRec(pScrn);
        return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Chipset revision: %d\n", pVia->ChipRev);

    /* Now handle the Display */
    if (flags & PROBE_DETECT)
        return TRUE;

    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * We support depths of 8, 16 and 24.
     * We support bpp of 8, 16, and 32.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
        free(pEnt);
        VIAFreeRec(pScrn);
        return FALSE;
    } else {
        switch (pScrn->depth) {
            case 8:
            case 16:
            case 24:
            case 32:
                /* OK */
                break;
            default:
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                           "Given depth (%d) is not supported by this driver\n",
                           pScrn->depth);
                free(pEnt);
                VIAFreeRec(pScrn);
                return FALSE;
        }
    }

    xf86PrintDepthBpp(pScrn);

    if (pScrn->depth == 32) {
        pScrn->depth = 24;
    }

    if (pScrn->depth > 8) {
        rgb zeros = { 0, 0, 0 };

        if (!xf86SetWeight(pScrn, zeros, zeros)) {
            free(pEnt);
            VIAFreeRec(pScrn);
            return FALSE;
        } else {
            /* TODO check weight returned is supported */
            ;
        }
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
        return FALSE;
    } else {
        /* We don't currently support DirectColor at > 8bpp */
        if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
                       " (%s) is not supported at depth %d.\n",
                       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
            free(pEnt);
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    /* We use a programmable clock */
    pScrn->progClock = TRUE;

    xf86CollectOptions(pScrn, NULL);

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
        pScrn->rgbBits = 6;

    if (!VIASetupDefaultOptions(pScrn)) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, VIAOptions);

    if (xf86GetOptValInteger(VIAOptions, OPTION_VIDEORAM, &pScrn->videoRam))
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Setting amount of VideoRAM to %d kB\n", pScrn->videoRam);

    if ((s = xf86GetOptValString(VIAOptions, OPTION_MODE_SWITCH_METHOD))) {
        if (!xf86NameCmp(s, "legacy")) {
            if (pVia->UseLegacyModeSwitch) {
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                           "Already using \"legacy\" as ModeSwitchMethod, "
                           "did not force anything.\n");
            }
            else {
                pVia->UseLegacyModeSwitch = TRUE;
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                           "Forced ModeSwitchMethod to \"legacy\".\n");
            }
        }
        else if (!xf86NameCmp(s, "new")) {
            if (pVia->UseLegacyModeSwitch) {
                pVia->UseLegacyModeSwitch = FALSE;
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                           "Forced ModeSwitchMethod to \"new\".\n");
            }
            else {
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                           "Already using \"new\" as ModeSwitchMethod, "
                           "did not force anything.\n");
            }
        } else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
                       "value for Option \"ModeSwitchMethod\".\n", s);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Valid options are \"legacy\" or \"new\".\n");
        }
    }

    /* When rotating, switch shadow framebuffer on and acceleration off. */
    if ((s = xf86GetOptValString(VIAOptions, OPTION_ROTATION_TYPE))) {
        if (!xf86NameCmp(s, "SWRandR")) {
            pVia->shadowFB = TRUE;
            pVia->NoAccel = TRUE;
            pVia->RandRRotation = TRUE;
            pVia->rotate = RR_Rotate_0;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen "
                       "RandR enabled, acceleration disabled\n");
        } else if (!xf86NameCmp(s, "HWRandR")) {
            pVia->shadowFB = TRUE;
            pVia->NoAccel = TRUE;
            pVia->RandRRotation = TRUE;
            pVia->rotate = RR_Rotate_0;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Hardware accelerated "
                       "rotating screen is not implemented. Using SW RandR.\n");
        } else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
                       "value for Option \"RotationType\".\n", s);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Valid options are \"SWRandR\" and \"HWRandR\".\n");
        }
    }


    /* When rotating, switch shadow framebuffer on and acceleration off. */
    if ((s = xf86GetOptValString(VIAOptions, OPTION_ROTATE))) {
        if (!xf86NameCmp(s, "CW")) {
            pVia->shadowFB = TRUE;
            pVia->NoAccel = TRUE;
            pVia->RandRRotation = TRUE;
            pVia->rotate = RR_Rotate_270;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen "
                       "clockwise -- acceleration is disabled.\n");
        } else if (!xf86NameCmp(s, "CCW")) {
            pVia->shadowFB = TRUE;
            pVia->NoAccel = TRUE;
            pVia->RandRRotation = TRUE;
            pVia->rotate = RR_Rotate_90;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen "
                       "counterclockwise -- acceleration is disabled.\n");
        } else if (!xf86NameCmp(s, "UD")) {
            pVia->shadowFB = TRUE;
            pVia->NoAccel = TRUE;
            pVia->RandRRotation = TRUE;
            pVia->rotate = RR_Rotate_180;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rotating screen "
                       "upside-down -- acceleration is disabled.\n");
        } else {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid"
                       "value for Option \"Rotate\".\n", s);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Valid options are \"CW\", \"CCW\" or  \"UD\".\n");
        }
    }

    from = (xf86GetOptValBool(VIAOptions, OPTION_SHADOW_FB, &pVia->shadowFB)
            ? X_CONFIG : X_DEFAULT);
    xf86DrvMsg(pScrn->scrnIndex, from, "Shadow framebuffer is %s.\n",
               pVia->shadowFB ? "enabled" : "disabled");

    /* Use hardware acceleration, unless on shadow framebuffer. */
    from = (xf86GetOptValBool(VIAOptions, OPTION_NOACCEL, &pVia->NoAccel)
            ? X_CONFIG : X_DEFAULT);
    if (!pVia->NoAccel && pVia->shadowFB) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Acceleration is "
                   "not supported when using shadow framebuffer.\n");
        pVia->NoAccel = TRUE;
        from = X_DEFAULT;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Hardware acceleration is %s.\n",
               !pVia->NoAccel ? "enabled" : "disabled");

    if (!pVia->NoAccel) {
        from = X_DEFAULT;
        if ((s = (char *)xf86GetOptValString(VIAOptions, OPTION_ACCELMETHOD))) {
            if (!xf86NameCmp(s, "EXA")) {
                from = X_CONFIG;
                pVia->useEXA = TRUE;
            } else if (!xf86NameCmp(s, "XAA")) {
                from = X_CONFIG;
                pVia->useEXA = FALSE;
            }
        }
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Using %s acceleration architecture.\n",
                   pVia->useEXA ? "EXA" : "XAA");

        //pVia->noComposite = FALSE;
        if (pVia->useEXA) {
            from = xf86GetOptValBool(VIAOptions, OPTION_EXA_NOCOMPOSITE,
                                     &pVia->noComposite) ? X_CONFIG : X_DEFAULT;
            xf86DrvMsg(pScrn->scrnIndex, from,
                       "EXA composite acceleration %s.\n",
                       !pVia->noComposite ? "enabled" : "disabled");

            //pVia->exaScratchSize = VIA_SCRATCH_SIZE / 1024;
            from = xf86GetOptValInteger(VIAOptions, OPTION_EXA_SCRATCH_SIZE,
                                        &pVia->exaScratchSize)
                    ? X_CONFIG : X_DEFAULT;
            xf86DrvMsg(pScrn->scrnIndex, from,
                       "EXA scratch area size is %d kB.\n",
                       pVia->exaScratchSize);
        }
    }

    /* Use a hardware cursor, unless on secondary or on shadow framebuffer. */
    from = X_DEFAULT;
    if (pVia->IsSecondary || pVia->shadowFB)
        pVia->hwcursor = FALSE;
    else if (xf86GetOptValBool(VIAOptions, OPTION_SWCURSOR,
                               &pVia->hwcursor)) {
        pVia->hwcursor = !pVia->hwcursor;
        from = X_CONFIG;
    }
    if (pVia->hwcursor)
        xf86DrvMsg(pScrn->scrnIndex, from, "Using hardware two-color "
                   "cursors and software full-color cursors.\n");
    else
        xf86DrvMsg(pScrn->scrnIndex, from, "Using software cursors.\n");

    //pVia->VQEnable = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLEVQ, &pVia->VQEnable)
            ? X_CONFIG : X_DEFAULT;
    if (from == X_CONFIG)
        pVia->VQEnable = !pVia->VQEnable;
    xf86DrvMsg(pScrn->scrnIndex, from,
               "GPU virtual command queue will be %s.\n",
               (pVia->VQEnable) ? "enabled" : "disabled");

    //pVia->DRIIrqEnable = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLEIRQ, &pVia->DRIIrqEnable)
            ? X_CONFIG : X_DEFAULT;
    if (from == X_CONFIG)
        pVia->DRIIrqEnable = !pVia->DRIIrqEnable;
    xf86DrvMsg(pScrn->scrnIndex, from,
               "DRI IRQ will be %s if DRI is enabled.\n",
               (pVia->DRIIrqEnable) ? "enabled" : "disabled");

    //pVia->agpEnable = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_AGP_DMA, &pVia->agpEnable)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from,
               "AGP DMA will be %s if DRI is enabled.\n",
               (pVia->agpEnable) ? "enabled" : "disabled");

    //pVia->dma2d = TRUE;
    if (pVia->agpEnable) {
        from = xf86GetOptValBool(VIAOptions, OPTION_2D_DMA, &pVia->dma2d)
                ? X_CONFIG : X_DEFAULT;
        if (from == X_CONFIG)
            pVia->dma2d = !pVia->dma2d;
        xf86DrvMsg(pScrn->scrnIndex, from, "AGP DMA will %sbe used for "
                   "2D acceleration.\n", (pVia->dma2d) ? "" : "not ");
    }
    //pVia->dmaXV = TRUE;
    from = xf86GetOptValBool(VIAOptions, OPTION_XV_DMA, &pVia->dmaXV)
            ? X_CONFIG : X_DEFAULT;
    if (from == X_CONFIG)
        pVia->dmaXV = !pVia->dmaXV;
    xf86DrvMsg(pScrn->scrnIndex, from, "PCI DMA will %sbe used for XV "
               "image transfer if DRI is enabled.\n",
               (pVia->dmaXV) ? "" : "not ");

    //pVia->useVBEModes = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_VBEMODES, &pVia->useVBEModes)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %senable VBE modes.\n",
               (pVia->useVBEModes) ? "" : "not ");

    //pVia->vbeSR = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_VBE_SAVERESTORE, &pVia->vbeSR)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "VBE VGA register save & restore "
               "will %sbe used\n\tif VBE modes are enabled.\n",
               (pVia->vbeSR) ? "" : "not ");

#ifdef HAVE_DEBUG
    //pVia->disableXvBWCheck = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_DISABLE_XV_BW_CHECK,
                             &pVia->disableXvBWCheck)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Xv Bandwidth check is %s.\n",
               pVia->disableXvBWCheck ? "disabled" : "enabled");
    if (pVia->disableXvBWCheck) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "You may get a \"snowy\" screen"
                   " when using the Xv overlay.\n");
    }
#endif

    //pVia->maxDriSize = 0;
    from = xf86GetOptValInteger(VIAOptions, OPTION_MAX_DRIMEM,
                                &pVia->maxDriSize)
            ? X_CONFIG : X_DEFAULT;
    if (pVia->maxDriSize > 0)
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Will impose a %d kB limit on video RAM reserved for DRI.\n",
                   pVia->maxDriSize);
    else
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Will not impose a limit on video RAM reserved for DRI.\n");

    //pVia->agpMem = AGP_SIZE / 1024;
    from = xf86GetOptValInteger(VIAOptions, OPTION_AGPMEM, &pVia->agpMem)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from,
               "Will try to allocate %d kB of AGP memory.\n", pVia->agpMem);

    /* ActiveDevice Option for device selection */
    //pVia->ActiveDevice = 0x00;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_ACTIVEDEVICE))) {
        if (strstr(s, "CRT"))
            pVia->ActiveDevice |= VIA_DEVICE_CRT;
        if (strstr(s, "LCD"))
            pVia->ActiveDevice |= VIA_DEVICE_LCD;
        if (strstr(s, "DFP"))
            pVia->ActiveDevice |= VIA_DEVICE_DFP;
        if (strstr(s, "TV"))
            pVia->ActiveDevice |= VIA_DEVICE_TV;
    }

    /* Digital Output Bus Width Option */
    pBIOSInfo = pVia->pBIOSInfo;
    pBIOSInfo->BusWidth = VIA_DI_12BIT;
    from = X_DEFAULT;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_BUSWIDTH))) {
        from = X_CONFIG;
        if (!xf86NameCmp(s, "12BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_12BIT;
        } else if (!xf86NameCmp(s, "24BIT")) {
            pBIOSInfo->BusWidth = VIA_DI_24BIT;
        }
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
               "Digital output bus width is %d bits.\n",
               (pBIOSInfo->BusWidth == VIA_DI_12BIT) ? 12 : 24);


    /* LCD Center/Expend Option */
    pBIOSInfo->Center = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_CENTER, &pBIOSInfo->Center)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "DVI Center is %s.\n",
               pBIOSInfo->Center ? "enabled" : "disabled");

    /* Panel Size Option */
    if ((s = xf86GetOptValString(VIAOptions, OPTION_PANELSIZE))) {
        ViaPanelGetNativeModeFromOption(pScrn, s);
        if (pBIOSInfo->Panel->NativeModeIndex != VIA_PANEL_INVALID) {
            ViaPanelModePtr mode = pBIOSInfo->Panel->NativeMode;

            DEBUG(xf86DrvMsg
                  (pScrn->scrnIndex, X_CONFIG, "Panel mode index is %d\n",
                   pBIOSInfo->Panel->NativeModeIndex));
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "Selected Panel Size is %dx%d\n", mode->Width,
                       mode->Height);
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT,
                   "Panel size is not selected from config file.\n");
    }

    /* Force the use of the Panel? */
    pBIOSInfo->ForcePanel = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_FORCEPANEL,
                             &pBIOSInfo->ForcePanel)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from,
               "Panel will %sbe forced.\n",
               pBIOSInfo->ForcePanel ? "" : "not ");

    pBIOSInfo->TVDotCrawl = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_TVDOTCRAWL,
                             &pBIOSInfo->TVDotCrawl)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "TV dotCrawl is %s.\n",
               pBIOSInfo->TVDotCrawl ? "enabled" : "disabled");

    /* TV Deflicker */
    pBIOSInfo->TVDeflicker = 0;
    from = xf86GetOptValInteger(VIAOptions, OPTION_TVDEFLICKER,
                                &pBIOSInfo->TVDeflicker)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "TV deflicker is set to %d.\n",
               pBIOSInfo->TVDeflicker);

    pBIOSInfo->TVType = TVTYPE_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVTYPE))) {
        if (!xf86NameCmp(s, "NTSC")) {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is NTSC.\n");
        } else if (!xf86NameCmp(s, "PAL")) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is PAL.\n");
        } else if (!xf86NameCmp(s, "480P")) {
            pBIOSInfo->TVType = TVTYPE_480P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is SDTV 480P.\n");
        } else if (!xf86NameCmp(s, "576P")) {
            pBIOSInfo->TVType = TVTYPE_576P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is SDTV 576P.\n");
        } else if (!xf86NameCmp(s, "720P")) {
            pBIOSInfo->TVType = TVTYPE_720P;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is HDTV 720P.\n");
        } else if (!xf86NameCmp(s, "1080I")) {
            pBIOSInfo->TVType = TVTYPE_1080I;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Type is HDTV 1080i.\n");
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "No default TV type is set.\n");
    }

    /* TV output signal Option */
    pBIOSInfo->TVOutput = TVOUTPUT_NONE;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVOUTPUT))) {
        if (!xf86NameCmp(s, "S-Video")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Signal is S-Video.\n");
        } else if (!xf86NameCmp(s, "Composite")) {
            pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Signal is Composite.\n");
        } else if (!xf86NameCmp(s, "SC")) {
            pBIOSInfo->TVOutput = TVOUTPUT_SC;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Output Signal is SC.\n");
        } else if (!xf86NameCmp(s, "RGB")) {
            pBIOSInfo->TVOutput = TVOUTPUT_RGB;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Signal is RGB.\n");
        } else if (!xf86NameCmp(s, "YCbCr")) {
            pBIOSInfo->TVOutput = TVOUTPUT_YCBCR;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Signal is YCbCr.\n");
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT,
                   "No default TV output signal type is set.\n");
    }

    /* TV DI Port */
    if ((s = xf86GetOptValString(VIAOptions, OPTION_TVDIPORT))) {
        if (!xf86NameCmp(s, "DVP0")) {
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Port is DVP0.\n");
        } else if (!xf86NameCmp(s, "DVP1")) {
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP1;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Port is DVP1.\n");
        } else if (!xf86NameCmp(s, "DFPHigh")) {
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DFPHIGH;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Port is DFPHigh.\n");
        } else if (!xf86NameCmp(s, "DFPLow")) {
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DFPLOW;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "TV Output Port is DFPLow.\n");
        }
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT,
                   "No default TV output port is set.\n");
    }

    VIAVidHWDiffInit(pScrn);

#ifdef HAVE_DEBUG
    //pVia->PrintVGARegs = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_PRINTVGAREGS,
                             &pVia->PrintVGARegs)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %sprint VGA registers.\n",
               pVia->PrintVGARegs ? "" : "not ");
    if (pVia->PrintVGARegs)
        ViaVgahwPrint(VGAHWPTR(pScrn)); /* Do this as early as possible */

    pVia->I2CScan = FALSE;
    from = xf86GetOptValBool(VIAOptions, OPTION_I2CSCAN, &pVia->I2CScan)
            ? X_CONFIG : X_DEFAULT;
    xf86DrvMsg(pScrn->scrnIndex, from, "Will %sscan I2C buses.\n",
               pVia->I2CScan ? "" : "not ");
#endif /* HAVE_DEBUG */

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "...Finished parsing config file options.\n");

    ViaCheckCardId(pScrn);

    /* I2CDevices Option for I2C Initialization */
    //pVia->I2CDevices = 0x00;
    if ((s = xf86GetOptValString(VIAOptions, OPTION_I2CDEVICES))) {
        if (strstr(s, "Bus1"))
            pVia->I2CDevices |= VIA_I2C_BUS1;
        if (strstr(s, "Bus2"))
            pVia->I2CDevices |= VIA_I2C_BUS2;
        if (strstr(s, "Bus3"))
            pVia->I2CDevices |= VIA_I2C_BUS3;
    }

    /* CRTC handling */
    if (!UMSCrtcInit(pScrn)) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    /* Initialize the colormap */
    Gamma zeros = { 0.0, 0.0, 0.0 };
    if (!xf86SetGamma(pScrn, zeros)) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

    /* Set up screen parameters. */
    pVia->Bpp = pScrn->bitsPerPixel >> 3;
    pVia->Bpl = pScrn->displayWidth * pVia->Bpp;

    /* This function fills in the Crtc fields for all the modes in the modes field of the ScrnInfoRec. */
    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

#ifdef USE_FB
    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }
#else
    /* Load bpp-specific modules. */
    switch (pScrn->bitsPerPixel) {
        case 8:
            mod = "cfb";
            break;
        case 16:
            mod = "cfb16";
            break;
        case 32:
            mod = "cfb32";
            break;
    }

    if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
        VIAFreeRec(pScrn);
        return FALSE;
    }

#endif

    if (!pVia->NoAccel) {
        if (pVia->useEXA) {
            XF86ModReqInfo req;
            int errmaj, errmin;

            memset(&req, 0, sizeof(req));
            req.majorversion = 2;
            req.minorversion = 0;
            if (!LoadSubModule(pScrn->module, "exa", NULL, NULL, NULL, &req,
                               &errmaj, &errmin)) {
                LoaderErrorMsg(NULL, "exa", errmaj, errmin);
                VIAFreeRec(pScrn);
                return FALSE;
            }
        }
        if (!xf86LoadSubModule(pScrn, "xaa")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    if (pVia->shadowFB) {
        if (!xf86LoadSubModule(pScrn, "shadowfb")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }
    return TRUE;
}

static void
LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
		LOCO * colors, VisualPtr pVisual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	CARD16 lut_r[256], lut_g[256], lut_b[256];
	int i, j, k, index;

	for (k = 0; k < xf86_config->num_crtc; k++) {
		xf86CrtcPtr crtc = xf86_config->crtc[k];

		switch (pScrn->depth) {
		case 15:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				for (j = 0; j < 8; j++) {
					lut_r[index * 8 + j] = colors[index].red << 8;
					lut_g[index * 8 + j] = colors[index].green << 8;
					lut_b[index * 8 + j] = colors[index].blue << 8;
				}
			}
			break;
		case 16:
			for (i = 0; i < numColors; i++) {
				index = indices[i];

				if (index <= 31) {
					for (j = 0; j < 8; j++) {
						lut_r[index * 8 + j] = colors[index].red << 8;
						lut_b[index * 8 + j] = colors[index].blue << 8;
					}
				}

				for (j = 0; j < 4; j++)
					lut_g[index * 4 + j] = colors[index].green << 8;
			}
			break;
		default:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				lut_r[index] = colors[index].red << 8;
				lut_g[index] = colors[index].green << 8;
				lut_b[index] = colors[index].blue << 8;
			}
			break;
		}

		/* Make the change through RandR */
#ifdef RANDR_12_INTERFACE
		RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
#else /*RANDR_12_INTERFACE*/
		crtc->funcs->gamma_set(crtc, lut_r, lut_g, lut_b, 256);
#endif
	}
}

static Bool
VIACreateScreenResources(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	VIAPtr pVia = VIAPTR(pScrn);

	pScreen->CreateScreenResources = pVia->CreateScreenResources;
	if (!(*pScreen->CreateScreenResources)(pScreen))
		return FALSE;
	pScreen->CreateScreenResources = VIACreateScreenResources;

	return TRUE;
}

static Bool
VIACloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	VIAPtr pVia = VIAPTR(pScrn);

	DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIACloseScreen\n"));

	viaExitAccel(pScreen);
	if (pVia->ShadowPtr) {
		free(pVia->ShadowPtr);
		pVia->ShadowPtr = NULL;
	}
	if (pVia->DGAModes) {
		free(pVia->DGAModes);
		pVia->DGAModes = NULL;
	}

	/* Is the display currently visible? */
	if (pScrn->vtSema)
		pScrn->LeaveVT(scrnIndex, 0);

#ifdef XF86DRI
	if (pVia->directRenderingType)
		VIADRICloseScreen(pScreen);
#endif

	pScrn->vtSema = FALSE;
	pScreen->CloseScreen = pVia->CloseScreen;
	return (*pScreen->CloseScreen) (scrnIndex, pScreen);
}

static Bool
VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
#ifdef XF86DRI
    drmVersionPtr drmVer;
    char *busId;
#endif
	int i;

    pScrn->pScreen = pScreen;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAScreenInit\n"));

	/* need to point to new screen on server regeneration */
	for (i = 0; i < xf86_config->num_crtc; i++)
		xf86_config->crtc[i]->scrn = pScrn;

	for (i = 0; i < xf86_config->num_output; i++)
		xf86_config->output[i]->scrn = pScrn;

    if (!UMSResourceManagement(pScrn))
		return FALSE;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		crtc->funcs->save(crtc);
	}

	xf86SetDesiredModes(pScrn);

	/* Darken the screen for aesthetic reasons and set the viewport. */
	xf86SaveScreen(pScreen, SCREEN_SAVER_ON);
	pScrn->AdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Blanked\n"));

	miClearVisualTypes();

    if (pScrn->bitsPerPixel > 8 && !pVia->IsSecondary) {
        if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    } else {
        if (!miSetVisualTypes(pScrn->depth,
                              miGetDefaultVisualMask(pScrn->depth),
                              pScrn->rgbBits, pScrn->defaultVisual))
            return FALSE;
        if (!miSetPixmapDepths())
            return FALSE;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Visuals set up\n"));

    pVia->directRenderingType = DRI_NONE;
#ifdef XF86DRI
    if (xf86LoaderCheckSymbol("DRICreatePCIBusID")) {
        busId = DRICreatePCIBusID(pVia->PciInfo);
    } else {
        busId = malloc(64);
        sprintf(busId, "PCI:%d:%d:%d",
#ifdef XSERVER_LIBPCIACCESS
                ((pVia->PciInfo->domain << 8) | pVia->PciInfo->bus),
                pVia->PciInfo->dev, pVia->PciInfo->func
#else
                ((pciConfigPtr)pVia->PciInfo->thisCard)->busnum,
                ((pciConfigPtr)pVia->PciInfo->thisCard)->devnum,
                ((pciConfigPtr)pVia->PciInfo->thisCard)->funcnum
#endif
               );
    }

	pVia->drmFD = drmOpen("via", busId);
	if (pVia->drmFD != -1) {
		if (NULL == (drmVer = drmGetVersion(pVia->drmFD))) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Could not get DRM"
						" driver version\n");
		} else {
			pVia->drmVerMajor = drmVer->version_major;
			pVia->drmVerMinor = drmVer->version_minor;
			pVia->drmVerPL = drmVer->version_patchlevel;
			drmFreeVersion(drmVer);

			if ((pVia->drmVerMajor < drmExpected.major) ||
				(pVia->drmVerMajor > drmCompat.major) ||
			   ((pVia->drmVerMajor == drmExpected.major) &&
				(pVia->drmVerMinor < drmExpected.minor))) {

				xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
						"[dri] Kernel drm is not compatible with this driver.\n"
						"[dri] Kernel drm version is %d.%d.%d, "
						"and I can work with versions %d.%d.x - %d.x.x.\n"
						"[dri] Update either this 2D driver or your kernel DRM. "
						"Disabling DRI.\n", pVia->drmVerMajor, pVia->drmVerMinor,
						pVia->drmVerPL, drmExpected.major, drmExpected.minor,
						drmCompat.major);
			} else {
				/* DRI2 or DRI1 support */
				if (pVia->drmVerMajor <= drmCompat.major) {
					drmClose(pVia->drmFD);
					if (VIADRI1ScreenInit(pScreen, busId)) {
						DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DRI 1 api supported\n"));
						pVia->directRenderingType = DRI_DRI1;
					}
				} else {
					DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DRI 2 api not supported yet\n"));
				}
			}
		}
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"[drm] Failed to open DRM device for %s: %s\n",
					busId, strerror(errno));
	}
#endif

    if (!VIAInternalScreenInit(scrnIndex, pScreen))
        return FALSE;

    xf86SetBlackWhitePixels(pScreen);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- B & W\n"));

    if (pScrn->bitsPerPixel > 8) {
        VisualPtr visual;

        visual = pScreen->visuals + pScreen->numVisuals;
        while (--visual >= pScreen->visuals) {
            if ((visual->class | DynamicClass) == DirectColor) {
                visual->offsetRed = pScrn->offset.red;
                visual->offsetGreen = pScrn->offset.green;
                visual->offsetBlue = pScrn->offset.blue;
                visual->redMask = pScrn->mask.red;
                visual->greenMask = pScrn->mask.green;
                visual->blueMask = pScrn->mask.blue;
            }
        }
    }
#ifdef USE_FB
    /* Must be after RGB ordering is fixed. */
    fbPictureInit(pScreen, 0, 0);
#endif

    if (!pVia->NoAccel && !UMSInitAccel(pScreen))
	    return FALSE;

    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
#if 0
    xf86SetSilkenMouse(pScreen);
#endif
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Backing store set up\n"));

    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- SW cursor set up\n"));

	if (pVia->shadowFB)
		ViaShadowFBInit(pScrn, pScreen);
	else
		VIADGAInit(pScreen);

	pScrn->vtSema = TRUE;
	pVia->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = VIACloseScreen;
	pScreen->SaveScreen = xf86SaveScreen;

	if (!xf86CrtcScreenInit(pScreen))
		return FALSE;

	xf86DPMSInit(pScreen, xf86DPMSSet, 0);
	DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- DPMS set up\n"));

    if (!miCreateDefColormap(pScreen))
        return FALSE;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Def Color map set up\n"));

    if (!xf86HandleColormaps(pScreen, 256, 8, LoadPalette, NULL,
                             CMAP_RELOAD_ON_MODE_SWITCH
                             | CMAP_PALETTED_TRUECOLOR))
        return FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Palette loaded\n"));

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Color maps etc. set up\n"));

	UMSAccelSetup(pScrn);

    viaInitVideo(pScreen);

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

#ifdef HAVE_DEBUG
    if (pVia->PrintVGARegs) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "VIAScreenInit: Printing VGA registers.\n");
        ViaVgahwPrint(VGAHWPTR(pScrn));
    }

    if (pVia->PrintTVRegs) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "VIAScreenInit: Printing TV registers.\n");
        ViaTVPrintRegs(pScrn);
    }
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "- Done\n"));
    return TRUE;
}

static int
VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    int width, height, displayWidth, shadowHeight;
    unsigned char *FBStart;

    xf86DrvMsg(scrnIndex, X_INFO, "VIAInternalScreenInit\n");

    displayWidth = pScrn->displayWidth;

    if ((pVia->rotate==RR_Rotate_90) || (pVia->rotate==RR_Rotate_270)) {
        height = pScrn->virtualX;
        width = pScrn->virtualY;
    } else {
        width = pScrn->virtualX;
        height = pScrn->virtualY;
    }

    if (pVia->RandRRotation)
        shadowHeight = max(width, height);
    else
        shadowHeight = height;

    if (pVia->shadowFB) {
        pVia->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pVia->ShadowPtr = malloc(pVia->ShadowPitch * shadowHeight);
        displayWidth = pVia->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pVia->ShadowPtr;
    } else {
        pVia->ShadowPtr = NULL;
        FBStart = pVia->FBBase;
    }

#ifdef USE_FB
    return fbScreenInit(pScreen, FBStart, width, height,
                        pScrn->xDpi, pScrn->yDpi, displayWidth,
                        pScrn->bitsPerPixel);
#else
    switch (pScrn->bitsPerPixel) {
        case 8:
            return cfbScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                                 pScrn->yDpi, displayWidth);
        case 16:
            return cfb16ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                                   pScrn->yDpi, displayWidth);
        case 32:
            return cfb32ScreenInit(pScreen, FBStart, width, height, pScrn->xDpi,
                                   pScrn->yDpi, displayWidth);
        default:
            xf86DrvMsg(scrnIndex, X_ERROR, "Internal error: invalid bpp (%d) "
                       "in VIAInternalScreenInit\n", pScrn->bitsPerPixel);
            return FALSE;
    }
#endif
    return TRUE;
}
