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

//#include "shadowfb.h"

#include "globals.h"
/*#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif

#include "svnversion.h"
*/
#include "via_driver.h"
/*#include "via_video.h"
#include "via.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6 
#include "xf86RAC.h"
#endif

#ifdef XF86DRI
#include "dri.h"
#endif
#include "via_vgahw.h"
*/
#include "via_id.h"

/* RandR support *

* Prototypes. *
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
#else * !XSERVER_LIBPCIACCESS *
static Bool VIAProbe(DriverPtr drv, int flags);
#endif

static Bool VIASetupDefaultOptions(ScrnInfoPtr pScrn);
static Bool VIAWriteMode(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool VIACloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool VIASaveScreen(ScreenPtr pScreen, int mode);
static Bool VIAScreenInit(int scrnIndex, ScreenPtr pScreen, int argc,
                          char **argv);
static int VIAInternalScreenInit(int scrnIndex, ScreenPtr pScreen);
static void VIADPMS(ScrnInfoPtr pScrn, int mode, int flags);
static const OptionInfoRec *VIAAvailableOptions(int chipid, int busid);
*/
//static Bool VIAMapMMIO(ScrnInfoPtr pScrn);
/*static Bool VIAMapFB(ScrnInfoPtr pScrn);
static void VIAUnmapMem(ScrnInfoPtr pScrn);


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

#endif * XSERVER_LIBPCIACCESS *

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

* Supported chipsets *
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

* Mapping a PCI device ID to a chipset family identifier. *
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

int gVIAEntityIndex = -1;

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
#ifdef HAVE_DEBUG * Don't document these three. *
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
    * Forcing use of panel is a last resort - don't document this one. *
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

    * Only be loaded once *
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
} * VIASetup *

#endif * XFree86LOADER *

static const OptionInfoRec *
VIAAvailableOptions(int chipid, int busid)
{
    return VIAOptions;
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
        scrn->SwitchMode = UMSSwitchMode;
        scrn->AdjustFrame = UMSAdjustFrame;
        scrn->EnterVT = UMSEnterVT;
        scrn->LeaveVT = UMSLeaveVT;
        scrn->FreeScreen = UMSFreeScreen;
        scrn->ValidMode = ViaValidMode;

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
#else * !XSERVER_LIBPCIACCESS *
static Bool
VIAProbe(DriverPtr drv, int flags)
{
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    int i;

    * sanity checks *
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
                pScrn->SwitchMode = UMSSwitchMode;
                pScrn->AdjustFrame = UMSAdjustFrame;
                pScrn->EnterVT = UMSEnterVT;
                pScrn->LeaveVT = UMSLeaveVT;
                pScrn->FreeScreen = UMSFreeScreen;
                pScrn->ValidMode = ViaValidMode;
                foundScreen = TRUE;
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

            * CLE266 supports dual-head; mark the entity as sharable. *
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

} * VIAProbe *
#endif * !XSERVER_LIBPCIACCESS *
*/

#ifdef XF86DRI
static void
kickVblank(ScrnInfoPtr pScrn)
{
    /*
     * Switching mode will clear registers that make vblank
     * interrupts happen. If the driver thinks interrupts
     * are enabled, make sure vblank interrupts go through.
     * registers are not documented in VIA docs.
     */

    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIADRIPtr pVIADRI = pVia->pDRIInfo->devPrivate;

    if (pVIADRI->irqEnabled) {
        hwp->writeCrtc(hwp, 0x11, hwp->readCrtc(hwp, 0x11) | 0x30);
    }
}
#endif

/*
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
    * Is there a function to do this for me? *
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

    * Disable vertical interpolation because the size of *
    * line buffer (limited to 800) is too small to do interpolation. *
    pVia->swov.maxWInterp = 800;
    pVia->swov.maxHInterp = 600;
    pVia->useLegacyVBE = TRUE;

    pVia->UseLegacyModeSwitch = FALSE;
    pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP1; 

    switch (pVia->Chipset) {
        case VIA_CLE266:    
            pVia->UseLegacyModeSwitch = TRUE;
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0; 
        case VIA_KM400:
            * IRQ is not broken on KM400A, but testing (pVia->ChipRev < 0x80)
             * is not enough to make sure we have an older, broken KM400. *
            pVia->DRIIrqEnable = FALSE;
            
            / The KM400 not working properly with new mode switch (See Ticket #301) *
            pVia->UseLegacyModeSwitch = TRUE;
            pBIOSInfo->TVDIPort = VIA_DI_PORT_DVP0; 
            break;
        case VIA_K8M800:
            pVia->DRIIrqEnable = FALSE;
            break;
        case VIA_PM800:
            * Use new mode switch to resolve many resolution and display bugs (switch to console) *
            * FIXME The video playing (XV) is not working correctly after turn on new mode switch *
            pVia->VideoEngine = VIDEO_ENGINE_CME;
            break;
        case VIA_VM800:
            * New mode switch resolve bug with gamma set #282 *
            * and with Xv after hibernate #240                *
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
            * FIXME: this needs to be tested *
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
*/

Bool
UMSCrtcInit(ScrnInfoPtr pScrn)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
    ClockRangePtr clockRanges;
    VIABIOSInfoPtr pBIOSInfo;
    int i;

    /* Read memory bandwidth from registers. */
    pVia->MemClk = hwp->readCrtc(hwp, 0x3D) >> 4;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "Detected MemClk %d\n", pVia->MemClk));
    if (pVia->MemClk >= VIA_MEM_END) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Unknown Memory clock: %d\n", pVia->MemClk);
        pVia->MemClk = VIA_MEM_END - 1;
    }
    pBIOSInfo = pVia->pBIOSInfo;
    pBIOSInfo->Bandwidth = ViaGetMemoryBandwidth(pScrn);

    if (pBIOSInfo->TVType == TVTYPE_NONE) {
        /* Use jumper to determine TV type. */
        if (hwp->readCrtc(hwp, 0x3B) & 0x02) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: PAL.\n"));
        } else {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: NTSC.\n"));
        }
    }

    if (!xf86LoadSubModule(pScrn, "i2c")) {
        VIAFreeRec(pScrn);
        return FALSE;
    } else {
        ViaI2CInit(pScrn);
    }

    if (!xf86LoadSubModule(pScrn, "ddc")) {
        VIAFreeRec(pScrn);
        return FALSE;
    } else {

        if (pVia->pI2CBus1) {
            pVia->DDC1 = xf86DoEEDID(pScrn->scrnIndex, pVia->pI2CBus1, TRUE);
            if (pVia->DDC1) {
                xf86PrintEDID(pVia->DDC1);
                xf86SetDDCproperties(pScrn, pVia->DDC1);
                DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                    "DDC pI2CBus1 detected a %s\n", DIGITAL(pVia->DDC1->features.input_type) ?
                    "DFP" : "CRT"));
            }
        }
    }

    ViaOutputsDetect(pScrn);
    if (!ViaOutputsSelect(pScrn)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No outputs possible.\n");
        VIAFreeRec(pScrn);
        return FALSE;
    }

    /* Might not belong here temporary fix for bug fix */
    ViaPreInitCRTCConfig(pScrn);

    if (!pVia->UseLegacyModeSwitch) {
        if (pBIOSInfo->Panel->IsActive)
            ViaPanelPreInit(pScrn);
    }

    pVia->pVbe = NULL;
    if (pVia->useVBEModes) {
        /* VBE doesn't properly initialise int10 itself. */
        if (xf86LoadSubModule(pScrn, "int10")
            && xf86LoadSubModule(pScrn, "vbe")) {
            pVia->pVbe = VBEExtendedInit(NULL, pVia->EntityIndex,
                                         SET_BIOS_SCRATCH |
                                         RESTORE_BIOS_SCRATCH);
        }

        if (!pVia->pVbe)
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "VBE initialisation failed."
                       " Using builtin code to set modes.\n");
    }

    if (pVia->pVbe) {

        if (!ViaVbeModePreInit(pScrn)) {
            VIAFreeRec(pScrn);
            return FALSE;
        }

    } else {
        int max_pitch, max_height;
        /* Add own modes. */
        ViaModesAttach(pScrn, pScrn->monitor);

        /*
         * Set up ClockRanges, which describe what clock ranges are
         * available, and what sort of modes they can be used for.
         */
        clockRanges = xnfalloc(sizeof(ClockRange));
        clockRanges->next = NULL;
        clockRanges->minClock = 20000;
        clockRanges->maxClock = 230000;

        clockRanges->clockIndex = -1;
        clockRanges->interlaceAllowed = TRUE;
        clockRanges->doubleScanAllowed = FALSE;

        switch (pVia->Chipset) {
            case VIA_CLE266:
            case VIA_KM400:
            case VIA_K8M800:
            case VIA_PM800:
            case VIA_VM800:
                max_pitch = 3344;
                max_height = 2508;
            case VIA_CX700:
            case VIA_K8M890:
            case VIA_P4M890:
            case VIA_P4M900:
                max_pitch = 8192/(pScrn->bitsPerPixel >> 3)-1;
                max_height = max_pitch;
                break;
            default:
                max_pitch = 16384/(pScrn->bitsPerPixel >> 3)-1;
                max_height = max_pitch;        
        }

        /*
         * xf86ValidateModes will check that the mode HTotal and VTotal values
         * don't exceed the chipset's limit if pScrn->maxHValue and
         * pScrn->maxVValue are set.  Since our VIAValidMode() already takes
         * care of this, we don't worry about setting them here.
         *
         * CLE266A:
         *   Max Line Pitch: 4080, (FB corruption when higher, driver problem?)
         *   Max Height: 4096 (and beyond)
         *
         * CLE266A: primary AdjustFrame can use only 24 bits, so we are limited
         * to 12x11 bits; 4080x2048 (~2:1), 3344x2508 (4:3), or 2896x2896 (1:1).
         * TODO Test CLE266Cx, KM400, KM400A, K8M800, CN400 please.
         *
         * We should be able to limit the memory available for a mode to 32 MB,
         * but xf86ValidateModes (or miScanLineWidth) fails to catch this
         * properly (apertureSize).
         */

        /* Select valid modes from those available. */
        i = xf86ValidateModes(pScrn, 
			      pScrn->monitor->Modes,	 /* List of modes available for the monitor */
                              pScrn->display->modes,	 /* List of mode names that the screen is requesting */
                              clockRanges,		 /* list of clock ranges */
                              NULL,			 /* list of line pitches */
                              256,			 /* minimum line pitch */
                              max_pitch,		 /* maximum line pitch */
                              16 * 8,			 /* pitch increment (in bits), we just want 16 bytes alignment */
                              128,			 /* min virtual height */
                              max_height,		 /* maximum virtual height */
                              pScrn->display->virtualX,	 /* virtual width */
                              pScrn->display->virtualY,  /* virtual height */
                              pVia->videoRambytes,       /* apertureSize */
                              LOOKUP_BEST_REFRESH);      /* lookup mode flags */

        if (i == -1) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "xf86ValidateModes failure\n");
            VIAFreeRec(pScrn);
            return FALSE;
        }

        /* This function deletes modes in the modes field of the ScrnInfoRec that have been marked as invalid. */
        xf86PruneDriverModes(pScrn);

        if (i == 0 || pScrn->modes == NULL) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    if (pVia->hwcursor) {
        if (!xf86LoadSubModule(pScrn, "ramdac")) {
            VIAFreeRec(pScrn);
            return FALSE;
        }
    }

    return TRUE;
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

    /* Memory mapped IO for video engine. */
    pVia->VidMapBase = pVia->MapBase + 0x200;
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

/*
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

        pVia->FBFreeStart = (pScrn->displayWidth * pScrn->bitsPerPixel >> 3) *
                pScrn->virtualY;
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

static void
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
*/

Bool
UMSPreInit(ScrnInfoPtr pScrn, int gVIAEntityIndex)
{
    MessageType from = X_PROBED;
    VIAPtr pVia = VIAPTR(pScrn);
    CARD8 videoRam;
    vgaHWPtr hwp;
#ifdef XSERVER_LIBPCIACCESS
    struct pci_device *vgaDevice = viaPciDeviceVga();
    struct pci_device *bridge = via_host_bridge();
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
        case VIA_KM400:
#ifdef XSERVER_LIBPCIACCESS
            pci_device_cfg_read_u8(bridge, &videoRam, 0xE1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 0), 0xE1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
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

    if (from == X_PROBED) {
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Probed amount of VideoRAM = %d kB\n", pScrn->videoRam);

        if (pScrn->videoRam < 16384) {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                       "Memory size detection failed: using 16 MB.\n");
            pScrn->videoRam = 16 << 10;
        }
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
    return TRUE;
}

/*
static Bool
VIAWriteMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAWriteMode\n"));

    pVia->OverlaySupported = FALSE;

    pScrn->vtSema = TRUE;

    if (!pVia->pVbe) {

        if (!vgaHWInit(pScrn, mode))
            return FALSE;

        if (pVia->UseLegacyModeSwitch) {
            if (!pVia->IsSecondary)
                ViaModePrimaryLegacy(pScrn, mode);
            else
                ViaModeSecondaryLegacy(pScrn, mode);
        } else {
            ViaCRTCInit(pScrn);
            ViaModeSet(pScrn, mode);
        }

    } else {

        if (!ViaVbeSetMode(pScrn, mode))
            return FALSE;
         *
         * FIXME: pVia->IsSecondary is not working here.  We should be able
         * to detect when the display is using the secondary head.
         * TODO: This should be enabled for other chipsets as well.
         *
        if (pVia->pBIOSInfo->Panel->IsActive) {
            switch (pVia->Chipset) {
                case VIA_P4M900:
                case VIA_VX800:
                case VIA_VX855:
                case VIA_VX900:
                     *
                     * Since we are using virtual, we need to adjust
                     * the offset to match the framebuffer alignment.
                     *
                    if (pScrn->displayWidth != mode->CrtcHDisplay)
                        ViaSecondCRTCHorizontalOffset(pScrn);
                    break;
            }
        }
    }

    * Enable the graphics engine. *
    if (!pVia->NoAccel) {
        VIAInitialize3DEngine(pScrn);
        viaInitialize2DEngine(pScrn);
    }

    pScrn->AdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}


static Bool
VIACloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "VIACloseScreen\n"));

    * Is the display currently visible? *
    if (pScrn->vtSema) {
#ifdef XF86DRI
        if (pVia->directRenderingEnabled)
            DRILock(screenInfo.screens[scrnIndex], 0);
#endif
        * Wait for hardware engine to idle before exiting graphical mode. *
        viaAccelSync(pScrn);

        * A soft reset avoids a 3D hang after X restart. *
        switch (pVia->Chipset) {
            case VIA_K8M890:
            case VIA_P4M900:
            case VIA_VX800:
            case VIA_VX855:
            case VIA_VX900:
                break;
            default :
                hwp->writeSeq(hwp, 0x1A, pVia->SavedReg.SR1A | 0x40);
                break;
        }

        if (!pVia->IsSecondary) {
            * Turn off all video activities. *
            viaExitVideo(pScrn);
            if (pVia->hwcursor)
                viaHideCursor(pScrn);
        }

        if (pVia->VQEnable)
            viaDisableVQ(pScrn);
    }
#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
        VIADRICloseScreen(pScreen);
#endif

    viaExitAccel(pScreen);
    if (pVia->ShadowPtr) {
        free(pVia->ShadowPtr);
        pVia->ShadowPtr = NULL;
    }
    if (pVia->DGAModes) {
        free(pVia->DGAModes);
        pVia->DGAModes = NULL;
    }

    if (pScrn->vtSema) {
        if (pVia->pVbe && pVia->vbeSR)
            ViaVbeSaveRestore(pScrn, MODE_RESTORE);
        else
            VIARestore(pScrn);

        vgaHWLock(hwp);
        VIAUnmapMem(pScrn);
    }
    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = pVia->CloseScreen;
    return (*pScreen->CloseScreen) (scrnIndex, pScreen);
}

static Bool
VIASaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
}

static void
VIADPMS(ScrnInfoPtr pScrn, int mode, int flags)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;

    if (pVia->pVbe) {
        ViaVbeDPMS(pScrn, mode, flags);
    } else {

        switch (mode) {
            case DPMSModeOn:

                if (pBIOSInfo->Lvds->IsActive)
                    ViaLVDSPower(pScrn, TRUE);

                if (pBIOSInfo->CrtActive)
                    ViaDisplayEnableCRT(pScrn);

                if (pBIOSInfo->Panel->IsActive)
                    ViaLCDPower(pScrn, TRUE);

                if (pBIOSInfo->TVActive)
                    ViaTVPower(pScrn, TRUE);

                if (pBIOSInfo->DfpActive)
                    ViaDFPPower(pScrn, TRUE);

                if (pBIOSInfo->Simultaneous->IsActive)
                    ViaDisplayEnableSimultaneous(pScrn);

                break;
            case DPMSModeStandby:
            case DPMSModeSuspend:
            case DPMSModeOff:

                if (pBIOSInfo->Lvds->IsActive)
                    ViaLVDSPower(pScrn, FALSE);

                if (pBIOSInfo->CrtActive)
                    ViaDisplayDisableCRT(pScrn);

                if (pBIOSInfo->Panel->IsActive)
                    ViaLCDPower(pScrn, FALSE);

                if (pBIOSInfo->TVActive)
                    ViaTVPower(pScrn, FALSE);

                if (pBIOSInfo->DfpActive)
                    ViaDFPPower(pScrn, FALSE);
                
                if (pBIOSInfo->Simultaneous->IsActive)
                    ViaDisplayDisableSimultaneous(pScrn);

                break;
            default:
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid DPMS mode %d\n",
                           mode);
                break;
        }
    }

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
}*/

static Bool
UMSSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr pVia = VIAPTR(pScrn);
    Bool ret;

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "UMSSwitchMode\n"));

#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
        DRILock(screenInfo.screens[scrnIndex], 0);
#endif

    viaAccelSync(pScrn);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled)
        VIADRIRingBufferCleanup(pScrn);
#endif

    if (pVia->VQEnable)
        viaDisableVQ(pScrn);

    ret = VIAWriteMode(pScrn, mode);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
        kickVblank(pScrn);
        VIADRIRingBufferInit(pScrn);
        DRIUnlock(screenInfo.screens[scrnIndex]);
    }
#endif
    return ret;
}

static void
UMSAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "UMSAdjustFrame %dx%d\n", x, y));

    if (pVia->pVbe) {
        ViaVbeAdjustFrame(scrnIndex, x, y, flags);
    } else {
        if (pVia->UseLegacyModeSwitch) {
            if (!pVia->IsSecondary)
                ViaFirstCRTCSetStartingAddress(pScrn, x, y);
            else
                ViaSecondCRTCSetStartingAddress(pScrn, x, y);
        } else {
            if (pVia->pBIOSInfo->FirstCRTC->IsActive)
                ViaFirstCRTCSetStartingAddress(pScrn, x, y);

            if (pVia->pBIOSInfo->SecondCRTC->IsActive)
                ViaSecondCRTCSetStartingAddress(pScrn, x, y);
        }
    }

    VIAVidAdjustFrame(pScrn, x, y);
}

static Bool
UMSEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    Bool ret;

    /* FIXME: Rebind AGP memory here. */
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "UMSEnterVT\n"));

    if (pVia->pVbe) {
        if (pVia->vbeSR)
            ViaVbeSaveRestore(pScrn, MODE_SAVE);
        else
            VIASave(pScrn);
        ret = ViaVbeSetMode(pScrn, pScrn->currentMode);
    } else {
        VIASave(pScrn);
        ret = VIAWriteMode(pScrn, pScrn->currentMode);
    }
    vgaHWUnlock(hwp);

    VIASaveScreen(pScrn->pScreen, SCREEN_SAVER_ON);

    /* A patch for APM suspend/resume, when HWCursor has garbage. */
    if (pVia->hwcursor)
        viaCursorRestore(pScrn);

    /* Restore video status. */
    if (!pVia->IsSecondary)
        viaRestoreVideo(pScrn);

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
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
    if (pVia->directRenderingEnabled) {
        DRIUnlock(screenInfo.screens[scrnIndex]);
    }
#endif

    return ret;
}

static void
UMSLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "UMSLeaveVT\n"));

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
        volatile drm_via_sarea_t *saPriv = (drm_via_sarea_t *)
                DRIGetSAREAPrivate(pScrn->pScreen);

        DRILock(screenInfo.screens[scrnIndex], 0);
        saPriv->ctxOwner = ~0;
    }
#endif

    viaAccelSync(pScrn);

    /* A soft reset helps to avoid a 3D hang on VT switch. */
    switch (pVia->Chipset) {
        case VIA_K8M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            break;
        default:
            hwp->writeSeq(hwp, 0x1A, pVia->SavedReg.SR1A | 0x40);
            break;
    }

#ifdef XF86DRI
    if (pVia->directRenderingEnabled) {
        VIADRIRingBufferCleanup(pScrn);
        viaDRIOffscreenSave(pScrn);
    }
#endif

    if (pVia->VQEnable)
        viaDisableVQ(pScrn);

    /* Save video status and turn off all video activities. */
    if (!pVia->IsSecondary)
        viaSaveVideo(pScrn);

    if (pVia->hwcursor)
        viaCursorStore(pScrn);

    if (pVia->pVbe && pVia->vbeSR)
        ViaVbeSaveRestore(pScrn, MODE_RESTORE);
    else
        VIARestore(pScrn);

    vgaHWLock(hwp);
}

/*
 * This only gets called when a screen is being deleted.  It does not
 * get called routinely at the end of a server generation.
 */
static void
UMSFreeScreen(int scrnIndex, int flags)
{
    DEBUG(xf86DrvMsg(scrnIndex, X_INFO, "UMSFreeScreen\n"));

    VIAFreeRec(xf86Screens[scrnIndex]);

    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
        vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

void
UMSInit(ScrnInfoPtr scrn)
{
    scrn->SwitchMode = UMSSwitchMode;
    scrn->AdjustFrame = UMSAdjustFrame;
    scrn->EnterVT = UMSEnterVT;
    scrn->LeaveVT = UMSLeaveVT;
    scrn->FreeScreen = UMSFreeScreen;
}
