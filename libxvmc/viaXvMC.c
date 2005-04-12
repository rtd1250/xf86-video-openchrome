/*****************************************************************************
 * VIA Unichrome XvMC extension client lib.
 *
 * Copyright (c) 2004 Thomas Hellström. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


/*
 *Author: Thomas Hellström, 2004.
 *TODO: Picture copying support.
 *Change: 0.9.3, Bugfix by Pascal Brisset.
 *Change: 0.10.1, Support for interlaced streams Thanks to Pascal Brisset.
 */

#undef WAITPAUSE

#include "viaXvMCPriv.h"
#include "viaLowLevel.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <fourcc.h>
#include <Xv.h>
#include <xf86drm.h>
#include <pthread.h>
#include "vldXvMC.h"

    
#define SAREAPTR(ctx) ((ViaXvMCSAreaPriv *)			\
		       (((CARD8 *)(ctx)->sAreaAddress) +	\
			(ctx)->sAreaPrivOffset))



static int error_base;
static int event_base;


#define FOURCC_XVMC (('C' << 24) + ('M' << 16) + ('V' << 8) + 'X')

static unsigned yOffs (ViaXvMCSurface *srf) 
{
    return srf->offsets[0];
}

static unsigned vOffs (ViaXvMCSurface *srf) 
{
    return srf->offsets[0] + srf->yStride * srf->height;
}

static unsigned uOffs (ViaXvMCSurface *srf) 
{
    return srf->offsets[0] + ( srf->yStride * srf->height) + 
	(srf->yStride >> 1) * (srf->height >> 1);
}


static void defaultQMatrices(ViaXvMCContext *ctx) 
{
    int i;

    static const char intra[64] = {
	8, 16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
        19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
        22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
        26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83
    };
    
    for( i=0; i<64; ++i) {
	ctx->intra_quantiser_matrix[i] = intra[i];
	ctx->non_intra_quantiser_matrix[i] = 16;
    }
    ctx->intraLoaded = 0;
    ctx->nonIntraLoaded = 0;
}


static void releaseDecoder(ViaXvMCContext *ctx,int clearCtx) 
{
    volatile ViaXvMCSAreaPriv *sAPriv;

    sAPriv = SAREAPTR(ctx);
    UNICHROME_UNLOCK(ctx->fd, UNICHROME_LOCK_DECODER1, sAPriv, ctx->drmcontext);
}


static int grabDecoder(	ViaXvMCContext *ctx, int *hadLastLock) 
{
    volatile ViaXvMCSAreaPriv *sAPriv =  SAREAPTR(ctx);
    int retFtx, lc;

    /*
     * Try to grab the decoder. If it is not available we will sleep until
     * it becomes available or for a maximum of 20 ms. 
     * Then try to grab it again, unless a timeout occured. If the decoder is
     * available, the lock should be reasonably fast.
     */

    if (ctx->haveDecoder) {
        flushXvMCLowLevel(&ctx->xl);  /* Ignore errors here. */
	/*
	  fprintf(stderr,"ViaXvMC: ERROR: Trying to re-lock decoder.\n"); */
	*hadLastLock = 1;
        return 0;
    }
    UNICHROME_LOCK(ctx->fd, UNICHROME_LOCK_DECODER1, sAPriv, ctx->drmcontext, lc,
		   retFtx);
    *hadLastLock = (ctx->drmcontext == lc);

    return retFtx;
}
	    
static void setupAttribDesc(Display *display, XvPortID port,
			    const ViaXvMCAttrHolder *attrib,
			    XvAttribute attribDesc[]) 
{
    XvAttribute *XvAttribs,*curAD;
    int num;
    unsigned i,j;

    XLockDisplay(display);
    XvAttribs = XvQueryPortAttributes(display, port, &num);
    for(i=0; i<attrib->numAttr; ++i) {
	curAD = attribDesc + i;
	curAD->flags = 0;
	curAD->min_value = 0;
	curAD->max_value = 0;
	curAD->name = NULL;
	for(j=0; j<num; ++j) {
	    if (attrib->attributes[i].attribute == 
		XInternAtom(display,XvAttribs[j].name,TRUE)) {
		*curAD = XvAttribs[j];
		curAD->name = strdup(XvAttribs[j].name);
		break;
	    }
	}
    }
    if (XvAttribs) XFree(XvAttribs);
    XUnlockDisplay(display);

}

static void releaseAttribDesc(int numAttr, XvAttribute attribDesc[]) 
{
    int i;

    for (i=0; i<numAttr; ++i) {
	if (attribDesc[i].name)
	    free(attribDesc[i].name);
    }
}
    
static Status releaseContextResources(Display *display, XvMCContext *context,
				      int freePrivate, Status errType)
{
    ViaXvMCContext *pViaXvMC = (ViaXvMCContext *) context->privData;

    switch(pViaXvMC->resources) {
    case context_lowLevel:
	closeXvMCLowLevel(&pViaXvMC->xl);
    case context_mutex:
	pthread_mutex_destroy(&pViaXvMC->ctxMutex);
    case context_sAreaMap:
	drmUnmap(pViaXvMC->sAreaAddress,pViaXvMC->sAreaSize);
    case context_fbMap:
	drmUnmap(pViaXvMC->fbAddress,pViaXvMC->fbSize);
    case context_mmioMap:
	drmUnmap(pViaXvMC->mmioAddress,pViaXvMC->mmioSize);
    case context_context:
	XLockDisplay(display);
	_xvmc_destroy_context(display, context);
	XUnlockDisplay(display);
    case context_fd:
	if (pViaXvMC->fd >= 0)
	    drmClose(pViaXvMC->fd);
	pViaXvMC->fd = -1;
	if (!freePrivate) break;
    default:
	free(pViaXvMC);
    }
    return errType;
}


Status XvMCCreateContext(Display *display, XvPortID port,
			 int surface_type_id, int width, int height, int flags,
			 XvMCContext *context) 
{  
    ViaXvMCContext *pViaXvMC;
    int priv_count;
    uint *priv_data;
    uint magic;
    unsigned i;
    Status ret;
    int major, minor;
    ViaXvMCCreateContextRec *tmpComm;
    drmVersionPtr drmVer;
    char curBusID[20];

    /* 
     * Verify Obvious things first 
     */

    if(context == NULL) {
	return XvMCBadContext;
    }

    if(!(flags & XVMC_DIRECT)) {
	fprintf(stderr,"Indirect Rendering not supported! Using Direct.\n");
    }

    /* 
     *FIXME: Check $DISPLAY for legal values here 
     */

    context->surface_type_id = surface_type_id;
    context->width = (unsigned short)((width + 15) & ~15);
    context->height = (unsigned short)((height + 15) & ~15);
    context->flags = flags;
    context->port = port;

    /* 
     *  Width, Height, and flags are checked against surface_type_id
     *  and port for validity inside the X server, no need to check
     *  here.
     */

    /* Allocate private Context data */
    context->privData = (void *)malloc(sizeof(ViaXvMCContext));
    if(!context->privData) {
	fprintf(stderr,"Unable to allocate resources for XvMC context.\n");
	return BadAlloc;
    }
  
    pViaXvMC = (ViaXvMCContext *)context->privData;
    pViaXvMC->resources = context_none;

    /* Verify the XvMC extension exists */

    XLockDisplay(display);
    if(! XvMCQueryExtension(display, &event_base,
			    &error_base)) {
	fprintf(stderr,"XvMC Extension is not available!\n");
	free(pViaXvMC);
	XUnlockDisplay(display);
	return BadAlloc;
    }

    /* Verify XvMC version */
    ret = XvMCQueryVersion(display, &major, &minor);
    if(ret) {
	fprintf(stderr,"XvMCQuery Version Failed, unable to determine "
		"protocol version\n");
    }
    XUnlockDisplay(display);

    /* FIXME: Check Major and Minor here */

    /* Check for drm */

    if(! drmAvailable()) {
	fprintf(stderr,"Direct Rendering is not avilable on this system!\n");
	return releaseContextResources(display, context, 1, BadAlloc);
    }

    /*
     * We don't know the BUSID. Have the X server tell it to us by faking
     * a working drm connection.
     */ 

    strncpy(curBusID,"NOBUSID",20);
    pViaXvMC->fd = -1;

    do {
	if (strcmp(curBusID,"NOBUSID")) {
	    if((pViaXvMC->fd = drmOpen("via",curBusID)) < 0) {
		fprintf(stderr,"DRM Device for via could not be opened.\n");
		return releaseContextResources(display, context, 1, BadAlloc);
	    }
	    pViaXvMC->resources = context_fd;

	    if (NULL == (drmVer = drmGetVersion(pViaXvMC->fd))) {
		fprintf(stderr, 
			"viaXvMC: Could not get drm version.");
		return releaseContextResources(display, context, 1, BadAlloc);
	    }
	    if (((drmVer->version_major != 2 ) || (drmVer->version_minor < 0))) {
		fprintf(stderr, 
			"viaXvMC: Kernel drm is not compatible with XvMC.\n"); 
		fprintf(stderr, 
			"viaXvMC: Kernel drm version: %d.%d.%d "
			"and I need at least version 2.0.0.\n"
			"Please update.\n",
			drmVer->version_major,drmVer->version_minor,
			drmVer->version_patchlevel); 
		drmFreeVersion(drmVer);
		return releaseContextResources(display, context, 1, BadAlloc);
	    } 
	    drmFreeVersion(drmVer);
	    drmGetMagic(pViaXvMC->fd,&magic);
	} else {
	    magic = 0;
	}
	context->flags = (unsigned long)magic;

	/*
	 * Pass control to the X server to create a drmContext for us, and
	 * validate the width / height and flags.
	 */

	XLockDisplay(display);
	if((ret = _xvmc_create_context(display, context, &priv_count, 
				       &priv_data))) {
	    XUnlockDisplay(display);
	    fprintf(stderr,"Unable to create XvMC Context.\n");
	    return releaseContextResources(display, context, 1, BadAlloc);
	}
	XUnlockDisplay(display);

	if(priv_count != (sizeof(ViaXvMCCreateContextRec) >> 2)) {
	    fprintf(stderr,"_xvmc_create_context() returned incorrect "
		    "data size!\n");
	    fprintf(stderr,"\tExpected %d, got %d\n",
		    (sizeof(ViaXvMCCreateContextRec) >> 2),
		    priv_count);
	    return releaseContextResources(display, context, 1, BadAlloc);
	}
	pViaXvMC->resources = context_context;

	tmpComm = (  ViaXvMCCreateContextRec *) priv_data;

	if ((tmpComm->major != VIAXVMC_MAJOR) ||
	    (tmpComm->minor != VIAXVMC_MINOR)) {
	    fprintf(stderr,"Version mismatch between the XFree86 via driver\n"
		    "and the XvMC library. Cannot continue!\n");
	    return releaseContextResources(display, context, 1, BadAlloc);
	}
      
	if (strncmp(curBusID, tmpComm->busIdString, 20)) {
	    releaseContextResources(display, context, 0, Success);
	    pViaXvMC->resources = context_none;
	    strncpy(curBusID, tmpComm->busIdString, 20);
	    XFree(priv_data);
	    continue;
	}
      
	if (!tmpComm->authenticated) {
	    XFree(priv_data);
	    return releaseContextResources(display, context, 1, BadAlloc);
	}
	    
    } while (pViaXvMC->fd < 0);

    pViaXvMC->ctxNo = tmpComm->ctxNo;
    pViaXvMC->drmcontext = tmpComm->drmcontext;
    pViaXvMC->fbOffset = tmpComm->fbOffset;
    pViaXvMC->fbSize = tmpComm->fbSize;
    pViaXvMC->mmioOffset = tmpComm->mmioOffset;
    pViaXvMC->mmioSize = tmpComm->mmioSize;
    pViaXvMC->sAreaOffset = tmpComm->sAreaOffset;
    pViaXvMC->sAreaSize = tmpComm->sAreaSize;
    pViaXvMC->sAreaPrivOffset = tmpComm->sAreaPrivOffset;
    pViaXvMC->decoderOn = 0;
    pViaXvMC->xvMCPort = tmpComm->xvmc_port;
    pViaXvMC->useAGP = tmpComm->useAGP;
    pViaXvMC->chipId = tmpComm->chipId;
    for (i=0; i<VIA_MAX_RENDSURF; ++i) {
	pViaXvMC->rendSurf[i] = 0;
    }
    strncpy(pViaXvMC->busIdString,tmpComm->busIdString,20);
    pViaXvMC->busIdString[20] = '\0';    
    pViaXvMC->attrib = tmpComm->initAttrs;
    pViaXvMC->lastSrfDisplaying = ~0;
    setupAttribDesc(display, port, &pViaXvMC->attrib, pViaXvMC->attribDesc);

fprintf("XvMCCreateContext: Chipid %08x, %08x\n", pViaXvMC->chipId, tmpComm->chipId);

    /* 
     * Must free the private data we were passed from X 
     */

    XFree(priv_data);
    
    /* 
     * Map the register memory 
     */

    if(drmMap(pViaXvMC->fd,pViaXvMC->mmioOffset,
	      pViaXvMC->mmioSize,&(pViaXvMC->mmioAddress)) < 0) {
	fprintf(stderr,"Unable to map the display chip mmio registers.\n");
	return releaseContextResources(display, context, 1, BadAlloc);
    }   
    pViaXvMC->resources = context_mmioMap;

    /* 
     * Map Framebuffer memory 
     */

    if(drmMap(pViaXvMC->fd,pViaXvMC->fbOffset,
	      pViaXvMC->fbSize,&(pViaXvMC->fbAddress)) < 0) {
	fprintf(stderr,"Unable to map XvMC Framebuffer.\n");
	return releaseContextResources(display, context, 1, BadAlloc);
    } 
    pViaXvMC->resources = context_fbMap;


    /*
     * Map XvMC Sarea and get the address of the HW lock.
     */

    if(drmMap(pViaXvMC->fd,pViaXvMC->sAreaOffset,
	      pViaXvMC->sAreaSize,&(pViaXvMC->sAreaAddress)) < 0) {
	fprintf(stderr,"Unable to map DRI SAREA.\n");
	return releaseContextResources(display, context, 1, BadAlloc);
    } 
    pViaXvMC->resources = context_sAreaMap;


    pViaXvMC->hwLock = (drmLockPtr) pViaXvMC->sAreaAddress;
    defaultQMatrices(pViaXvMC);
    pViaXvMC->chromaIntraLoaded = 1;
    pViaXvMC->chromaNonIntraLoaded = 1;
    pViaXvMC->yStride = (width + 31) & ~31;
    pViaXvMC->haveDecoder = 0;
    pViaXvMC->attribChanged = 1;
    pViaXvMC->haveXv = 0;
    pViaXvMC->port = context->port;
    pthread_mutex_init(&pViaXvMC->ctxMutex,NULL);
    pViaXvMC->resources = context_mutex;
    pViaXvMC->timeStamp = 0;

    if (initXvMCLowLevel(&pViaXvMC->xl, pViaXvMC->fd, &pViaXvMC->drmcontext,
			 pViaXvMC->hwLock, pViaXvMC->mmioAddress, 
			 pViaXvMC->fbAddress, pViaXvMC->useAGP, pViaXvMC->chipId )) {
	fprintf(stderr,"ViaXvMC: Could not allocate timestamp blit area\n");
	return releaseContextResources(display, context, 1, BadAlloc);
    }       
    pViaXvMC->resources = context_lowLevel;
    setAGPSyncLowLevel(&pViaXvMC->xl, 1, 0);
    hwlLock(&pViaXvMC->xl,1); 
    setLowLevelLocking(&pViaXvMC->xl,0);
    viaVideoSubPictureOffLocked(&pViaXvMC->xl); 
    flushXvMCLowLevel(&pViaXvMC->xl);  /* Ignore errors here. */
    setLowLevelLocking(&pViaXvMC->xl,1);
    hwlUnlock(&pViaXvMC->xl,1); 
    return Success;
}


Status XvMCDestroyContext(Display *display, XvMCContext *context) 
{
    ViaXvMCContext *pViaXvMC;


    if(context == NULL) {
	return (error_base + XvMCBadContext);
    }
    if(NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }


    /*
     * Release decoder if we have it. In case of crash or termination
     * before XvMCDestroyContext, the X server will take care of this.
     */

    releaseAttribDesc(pViaXvMC->attrib.numAttr,pViaXvMC->attribDesc);
    releaseDecoder(pViaXvMC,1);
    return releaseContextResources(display, context, 1, Success);
}

Status XvMCCreateSurface( Display *display, XvMCContext *context,
			  XvMCSurface *surface) 
{
    ViaXvMCContext *pViaXvMC;
    ViaXvMCSurface *pViaSurface;
    int priv_count;
    unsigned *priv_data;
    unsigned i;
    Status ret;

    if((surface == NULL) || (context == NULL) || (display == NULL)){
	return BadValue;
    }
  
    pViaXvMC = (ViaXvMCContext *)context->privData;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    if(pViaXvMC == NULL) {
        pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return (error_base + XvMCBadContext);
    }

    pViaSurface = surface->privData = (ViaXvMCSurface *)malloc(sizeof(ViaXvMCSurface));
    if(!surface->privData) {
        pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadAlloc;
    }
    XLockDisplay(display);
    if((ret = _xvmc_create_surface(display, context, surface,
				   &priv_count, &priv_data))) {
	XUnlockDisplay(display);
	free(pViaSurface);
	fprintf(stderr,"Unable to create XvMC Surface.\n");
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return ret;
    }
    XUnlockDisplay(display);

    pViaSurface->srfNo = priv_data[0];

    /*
     * Store framebuffer offsets to the buffers allocated for this surface.
     * For some chipset revisions, surfaces may be double-buffered.
     */

    pViaSurface->numBuffers = priv_data[1];
    for (i=0; i < pViaSurface->numBuffers; ++i) {
	pViaSurface->offsets[i] = priv_data[i+2];
    }
    pViaSurface->curBuf = 0;


    /* Free data returned from xvmc_create_surface */

    XFree(priv_data);

    pViaSurface->width = context->width;
    pViaSurface->height = context->height;
    pViaSurface->yStride = pViaXvMC->yStride;
    pViaSurface->privContext = pViaXvMC;
    pViaSurface->privSubPic = NULL;
    pViaSurface->needsSync = 0;
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}

Status XvMCDestroySurface(Display *display, XvMCSurface *surface) 
{
    ViaXvMCSurface *pViaSurface;

    if((display == NULL) || (surface == NULL)) {
	return BadValue;
    }
    if(surface->privData == NULL) {
	return (error_base + XvMCBadSurface);
    }

    pViaSurface = (ViaXvMCSurface *)surface->privData;

    XLockDisplay(display);
    _xvmc_destroy_surface(display,surface);
    XUnlockDisplay(display);

    free(pViaSurface);
    surface->privData = NULL;
    return Success;
}

Status XvMCPutSlice2(Display *display,XvMCContext *context, char *slice,
		     int nBytes, int sliceCode) 
{
    ViaXvMCContext *pViaXvMC;
    CARD32 sCode = 0x00010000 | (sliceCode & 0xFF) << 24;

    if((display == NULL) || (context == NULL)) {
	return BadValue;
    }
    if(NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }
    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (!pViaXvMC->haveDecoder) {
	fprintf(stderr,"XvMCPutSlice: This context does not own decoder!\n");
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadAlloc;
    }

    viaMpegWriteSlice(&pViaXvMC->xl, (CARD8 *)slice, nBytes, sCode);

    flushPCIXvMCLowLevel(&pViaXvMC->xl); 
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}

Status XvMCPutSlice(Display *display,XvMCContext *context, char *slice,
		    int nBytes) 
{
    ViaXvMCContext *pViaXvMC;

    if((display == NULL) || (context == NULL)) {
	return BadValue;
    }
    if(NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }
    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    if (!pViaXvMC->haveDecoder) {
	fprintf(stderr,"XvMCPutSlice: This context does not own decoder!\n");
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadAlloc;
    }

    viaMpegWriteSlice(&pViaXvMC->xl, (CARD8 *)slice, nBytes, 0);
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}


static Status updateXVOverlay(Display *display,ViaXvMCContext *pViaXvMC, 
			      ViaXvMCSurface *pViaSurface, Drawable draw,
			      short srcx, short srcy, unsigned short srcw, 
			      unsigned short srch,short destx,short desty,
			      unsigned short destw,unsigned short desth) 
{
    ViaXvMCCommandBuffer buf;
    ViaXvMCSubPicture *pViaSubPic;
    Status ret;

    if (!pViaXvMC->haveXv) {
	pViaXvMC->xvImage = 
	    XvCreateImage(display,pViaXvMC->port,FOURCC_XVMC,
			  (char *)&buf,pViaSurface->width,
			  pViaSurface->height);
	pViaXvMC->gc = XCreateGC(display,draw,0,0);
	pViaXvMC->haveXv = 1;
    }    
    pViaXvMC->draw = draw;
    pViaXvMC->xvImage->data = (char *)&buf;
    
    buf.command = (pViaXvMC->attribChanged) ? 
	VIA_XVMC_COMMAND_FDISPLAY : VIA_XVMC_COMMAND_DISPLAY;
    buf.ctxNo = pViaXvMC->ctxNo | VIA_XVMC_VALID;
    buf.srfNo = pViaSurface->srfNo | VIA_XVMC_VALID;
    pViaSubPic = pViaSurface->privSubPic;
    buf.subPicNo = ((NULL == pViaSubPic) ? 0 : pViaSubPic->srfNo  ) 
	| VIA_XVMC_VALID;
    buf.attrib = pViaXvMC->attrib;
    
    XLockDisplay(display);

    if ((ret = XvPutImage(display,pViaXvMC->port,draw,pViaXvMC->gc,
			  pViaXvMC->xvImage,srcx,srcy,srcw,srch,
			  destx,desty,destw,desth))) {
	XUnlockDisplay(display);
	return ret;
    }
    XSync(display, 0);
    XUnlockDisplay(display);
    pViaXvMC->attribChanged = 0;
    return Success;
}

Status XvMCPutSurface(Display *display,XvMCSurface *surface,Drawable draw,
		      short srcx, short srcy, unsigned short srcw, 
		      unsigned short srch,short destx,short desty,
		      unsigned short destw,unsigned short desth, int flags) 
{
    /*
     * This function contains some hairy locking logic. What we really want to
     * do is to flip the picture ASAP, to get a low latency and smooth playback.
     * However, if somebody else used the overlay since we used it last or if it is
     * our first time, we'll have to call X to update the overlay first. Otherwise 
     * we'll do the overlay update once we've flipped. Since we release the hardware
     * lock when we call X, X needs to verify using the SAREA that nobody else flipped
     * in a picture between the lock release and the X server control. Similarly
     * when the overlay update returns, we have to make sure that we still own the
     * overlay.
     */

    ViaXvMCSurface *pViaSurface;
    ViaXvMCContext *pViaXvMC;
    ViaXvMCSubPicture *pViaSubPic;
    volatile ViaXvMCSAreaPriv *sAPriv;
    Status ret;
    unsigned dispSurface, lastSurface;
    int overlayUpdated;

    if((display == NULL) || (surface == NULL)) {
	return BadValue;
    }
    if(NULL == (pViaSurface = surface->privData )) {
	return (error_base + XvMCBadSurface);
    }
    if (NULL == (pViaXvMC = pViaSurface->privContext)) {
	return (error_base + XvMCBadContext);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    pViaSubPic = pViaSurface->privSubPic;
    sAPriv = SAREAPTR( pViaXvMC );
    hwlLock(&pViaXvMC->xl,1); 
    
    /*
     * Put a surface ID in the SAREA to "authenticate" to the 
     * X server.
     */

    dispSurface = sAPriv->XvMCDisplaying[pViaXvMC->xvMCPort];
    lastSurface = pViaXvMC->lastSrfDisplaying;
    sAPriv->XvMCDisplaying[pViaXvMC->xvMCPort] = 
	pViaXvMC->lastSrfDisplaying = pViaSurface->srfNo | VIA_XVMC_VALID;
    overlayUpdated = 0;

    viaVideoSetSWFLipLocked(&pViaXvMC->xl, yOffs(pViaSurface), uOffs(pViaSurface), 
			    vOffs(pViaSurface));

    if (lastSurface != dispSurface) {
	hwlUnlock(&pViaXvMC->xl,1);

	/*
	 * We weren't the last to display. Update the overlay before flipping.
	 */

	flushPCIXvMCLowLevel(&pViaXvMC->xl);
	ret = updateXVOverlay(display,pViaXvMC,pViaSurface,draw,srcx,srcy,srcw, 
			      srch,destx,desty,destw,desth);
	if (ret) {
	    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	    return ret;
	}

	hwlLock(&pViaXvMC->xl,1);
	overlayUpdated = 1;
	if (pViaXvMC->lastSrfDisplaying != sAPriv->XvMCDisplaying[pViaXvMC->xvMCPort]) {

	    /*
	     * Race. Somebody beat us to the port.
	     */
	  
	    hwlUnlock(&pViaXvMC->xl,1);
	    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	    return BadAccess;
	}
    } 
    setLowLevelLocking(&pViaXvMC->xl,0);

    /*
     * Subpictures
     */

    if (NULL != pViaSubPic) {
	if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] 
	    != (pViaSubPic->srfNo | VIA_XVMC_VALID)) {
	    sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] = 
		pViaSubPic->srfNo | VIA_XVMC_VALID; 
	    viaVideoSubPictureLocked(&pViaXvMC->xl, pViaSubPic);
	}  
    } else {
	if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] & VIA_XVMC_VALID) {
	    viaVideoSubPictureOffLocked(&pViaXvMC->xl);
	    sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] &= ~VIA_XVMC_VALID;
	}  
    }

    /*
     * Flip
     */

    viaVideoSWFlipLocked(&pViaXvMC->xl, flags, pViaSurface->progressiveSequence);
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    setLowLevelLocking(&pViaXvMC->xl,1);
    hwlUnlock(&pViaXvMC->xl,1);

    if (overlayUpdated) {
        pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return Success;
    }
    
    /*
     * Update overlay
     */

    ret = updateXVOverlay(display,pViaXvMC,pViaSurface,draw,srcx,srcy,srcw, 
			  srch,destx,desty,destw,desth);
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return ret;

}

void debugControl(const XvMCMpegControl *control) {
    printf("BVMV_range: %u\n",control->BVMV_range);
    printf("BHMV_range: %u\n",control->BHMV_range);
    printf("FVMV_range: %u\n",control->FVMV_range);
    printf("FHMV_range: %u\n",control->FHMV_range);
    printf("picture_structure: %u\n", control->picture_structure);
    printf("intra_dc_precision: %u\n", control->intra_dc_precision);
    printf("picture_coding_type: %u\n", control->picture_coding_type);
    printf("mpeg_coding: %u\n", control->mpeg_coding);
    printf("flags: 0x%x\n", control->flags);
}

    


Status XvMCBeginSurface(Display *display,
			XvMCContext *context,
			XvMCSurface *target_surface,
			XvMCSurface *past_surface,
			XvMCSurface *future_surface,
			const XvMCMpegControl *control) 
{
    ViaXvMCSurface *targS,*futS,*pastS;
    ViaXvMCContext *pViaXvMC;
    int hadDecoderLast;
    CARD32 timeStamp;

    if((display == NULL) || (context == NULL) || (target_surface == NULL)) {
	return BadValue;
    }
     
    pViaXvMC = context->privData;     

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (grabDecoder(pViaXvMC, &hadDecoderLast)) {
      pthread_mutex_unlock( &pViaXvMC->ctxMutex );
      return BadAlloc;
    }
    pViaXvMC->haveDecoder = 1;
 
    /*
     * We need to wait for decoder idle at next flush, since hardware doesn't queue 
     * beginsurface requests until the decoder is idle. This is  
     * done by waiting on the last previous timestamp, or if there was another context
     * having the decoder before us, by emitting a new one.
     */

    if (pViaXvMC->useAGP) {
        if (!hadDecoderLast || pViaXvMC->timeStamp == 0) {
	    timeStamp = viaDMATimeStampLowLevel(&pViaXvMC->xl);
	    if (flushXvMCLowLevel(&pViaXvMC->xl)) {
	        releaseDecoder(pViaXvMC, 0);
		pthread_mutex_unlock( &pViaXvMC->ctxMutex );
		return BadAlloc;
	    } 	      
	    pViaXvMC->timeStamp = timeStamp;
	} else {
	    timeStamp = pViaXvMC->timeStamp;
        }
	setAGPSyncLowLevel(&pViaXvMC->xl, 1, timeStamp);
    }

    if (!hadDecoderLast || !pViaXvMC->decoderOn) {
	pViaXvMC->intraLoaded = 0;
	pViaXvMC->nonIntraLoaded = 0;
    }

    viaMpegReset(&pViaXvMC->xl);

    targS = (ViaXvMCSurface *)target_surface->privData;
    futS = NULL;
    pastS = NULL;

 
    pViaXvMC->rendSurf[0] = targS->srfNo | VIA_XVMC_VALID;
    if (future_surface) {
	futS = (ViaXvMCSurface *)future_surface->privData;
	futS->needsSync = 0;
    }
    if (past_surface) {	
	pastS = (ViaXvMCSurface *)past_surface->privData;
	pastS->needsSync = 0;
    }


    targS->progressiveSequence = (control->flags & XVMC_PROGRESSIVE_SEQUENCE); 
    targS->privSubPic = NULL;
    
    viaMpegSetSurfaceStride(&pViaXvMC->xl,pViaXvMC);

    viaMpegSetFB(&pViaXvMC->xl,0,yOffs(targS),uOffs(targS),vOffs(targS));
    if (past_surface) {
	viaMpegSetFB(&pViaXvMC->xl,1,yOffs(pastS),uOffs(pastS),vOffs(pastS));
    } else {
	viaMpegSetFB(&pViaXvMC->xl,1,0,0,0);
    }
    
    if (future_surface) {
	viaMpegSetFB(&pViaXvMC->xl,2,yOffs(futS),uOffs(futS),vOffs(futS));
    } else {
	viaMpegSetFB(&pViaXvMC->xl,2,0,0,0);
    }

    viaMpegBeginPicture(&pViaXvMC->xl,pViaXvMC,context->width,context->height,control);
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    targS->needsSync = 1;
    targS->syncMode = LL_MODE_DECODER_IDLE;
    pViaXvMC->decoderOn = 1;
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}
    
  
Status XvMCSyncSurface(Display *display,XvMCSurface *surface)
{
    ViaXvMCSurface *pViaSurface;
    ViaXvMCContext *pViaXvMC;
    unsigned i;

    if((display == NULL) || (surface == NULL)) {
	return BadValue;
    }
    if(surface->privData == NULL) {
	return (error_base + XvMCBadSurface);
    }
    
    pViaSurface = (ViaXvMCSurface *)surface->privData;
    pViaXvMC = pViaSurface->privContext;

    if(pViaXvMC == NULL) {
	return (error_base + XvMCBadSurface);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    if (pViaSurface->needsSync) {
        CARD32 timeStamp = pViaSurface->timeStamp;
	int syncMode = pViaSurface->syncMode;

	if (pViaXvMC->useAGP) {

	    syncMode = (pViaSurface->syncMode == LL_MODE_2D || 
			pViaSurface->timeStamp < pViaXvMC->timeStamp) ? 
		LL_MODE_2D : LL_MODE_DECODER_IDLE;
	    if (pViaSurface->syncMode != LL_MODE_2D) 
	        timeStamp = pViaXvMC->timeStamp;

	} else if (syncMode != LL_MODE_2D && 
		   pViaXvMC->rendSurf[0] != (pViaSurface->srfNo | VIA_XVMC_VALID)) {

	  pViaSurface->needsSync = 0;
	  pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	  return Success;
	}

	if (syncXvMCLowLevel(&pViaXvMC->xl, syncMode, 1, 
			     pViaSurface->timeStamp)) {
	    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	    return BadValue;
	}
	pViaSurface->needsSync = 0;
    }

    if (pViaXvMC->rendSurf[0] == (pViaSurface->srfNo | VIA_XVMC_VALID)) {
	pViaSurface->needsSync = 0;
	for (i=0; i<VIA_MAX_RENDSURF; ++i) {
	    pViaXvMC->rendSurf[i] = 0;
	}
    }

    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}

Status XvMCLoadQMatrix(Display *display, XvMCContext *context,
		       const XvMCQMatrix *qmx) 
{	
    ViaXvMCContext
	*pViaXvMC;

    if((display == NULL) || (context == NULL)) {
	return BadValue;
    }
    if(NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (qmx->load_intra_quantiser_matrix) {
	memcpy(pViaXvMC->intra_quantiser_matrix,
	       qmx->intra_quantiser_matrix,
	       sizeof(qmx->intra_quantiser_matrix));
	pViaXvMC->intraLoaded = 0;
    }

    if (qmx->load_non_intra_quantiser_matrix) {
	memcpy(pViaXvMC->non_intra_quantiser_matrix,
	       qmx->non_intra_quantiser_matrix,
	       sizeof(qmx->non_intra_quantiser_matrix));
	pViaXvMC->nonIntraLoaded = 0;
    }

    if (qmx->load_chroma_intra_quantiser_matrix) {
	memcpy(pViaXvMC->chroma_intra_quantiser_matrix,
	       qmx->chroma_intra_quantiser_matrix,
	       sizeof(qmx->chroma_intra_quantiser_matrix));
	pViaXvMC->chromaIntraLoaded = 0;
    }

    if (qmx->load_chroma_non_intra_quantiser_matrix) {
	memcpy(pViaXvMC->chroma_non_intra_quantiser_matrix,
	       qmx->chroma_non_intra_quantiser_matrix,
	       sizeof(qmx->chroma_non_intra_quantiser_matrix));
	pViaXvMC->chromaNonIntraLoaded = 0;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );

    return Success;
}    

/*
 * Below, we provide functions unusable for this implementation, but for
 * standard completeness.
 */

  
Status XvMCRenderSurface
(
    Display *display,
    XvMCContext *context,
    unsigned int picture_structure,
    XvMCSurface *target_surface,
    XvMCSurface *past_surface,
    XvMCSurface *future_surface,
    unsigned int flags,
    unsigned int num_macroblocks,
    unsigned int first_macroblock,
    XvMCMacroBlockArray *macroblock_array,
    XvMCBlockArray *blocks
    ) 
{
    return (error_base + XvMCBadContext);
}

Status XvMCCreateBlocks 
(
    Display *display, 
    XvMCContext *context,
    unsigned int num_blocks,
    XvMCBlockArray * block
    )
{
    return (error_base + XvMCBadContext);
}

Status XvMCDestroyBlocks (Display *display, XvMCBlockArray * block) 
{
    return Success;
}

Status XvMCCreateMacroBlocks 
(
    Display *display, 
    XvMCContext *context,
    unsigned int num_blocks,
    XvMCMacroBlockArray * blocks
    )
{
    return (error_base + XvMCBadContext);
}

Status XvMCDestroyMacroBlocks(Display *display, XvMCMacroBlockArray * block)
{
    return (error_base + XvMCBadContext);
}

Status XvMCCreateSubpicture( Display *display, 
			     XvMCContext *context,
			     XvMCSubpicture *subpicture,
			     unsigned short width,
			     unsigned short height,
			     int xvimage_id) 
{
    ViaXvMCContext *pViaXvMC;
    ViaXvMCSubPicture *pViaSubPic;
    int priv_count;
    unsigned *priv_data;
    Status ret;

    if((subpicture == NULL) || (context == NULL) || (display == NULL)){
	return BadValue;
    }
  
    pViaXvMC = (ViaXvMCContext *)context->privData;
    if(pViaXvMC == NULL) {
	return (error_base + XvMCBadContext);
    }

    subpicture->privData = (ViaXvMCSubPicture *)
	malloc(sizeof(ViaXvMCSubPicture));
    if(!subpicture->privData) {
	return BadAlloc;
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    subpicture->width = context->width;
    subpicture->height = context->height;
    subpicture->xvimage_id = xvimage_id;
    pViaSubPic = (ViaXvMCSubPicture *)subpicture->privData;

    XLockDisplay(display);
    if((ret = _xvmc_create_subpicture(display, context, subpicture,
				      &priv_count, &priv_data))) {
	XUnlockDisplay(display);
	free(pViaSubPic);
	fprintf(stderr,"Unable to create XvMC Subpicture.\n");
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return ret;
    }
    XUnlockDisplay(display);


    subpicture->num_palette_entries = VIA_SUBPIC_PALETTE_SIZE;
    subpicture->entry_bytes = 3;
    strncpy(subpicture->component_order,"YUV",4); 
    pViaSubPic->srfNo = priv_data[0];
    pViaSubPic->offset = priv_data[1];
    pViaSubPic->stride = (subpicture->width + 31) & ~31;
    pViaSubPic->privContext = pViaXvMC;
    pViaSubPic->ia44 = (xvimage_id == FOURCC_IA44);
    pViaSubPic->needsSync = 0;

    /* Free data returned from _xvmc_create_subpicture */

    XFree(priv_data);
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}

Status
XvMCSetSubpicturePalette (Display *display, XvMCSubpicture *subpicture, 
			  unsigned char *palette) 
{    
    ViaXvMCSubPicture *pViaSubPic;
    ViaXvMCContext *pViaXvMC;
    volatile ViaXvMCSAreaPriv *sAPriv;
    unsigned i;
    CARD32 tmp;

    if((subpicture == NULL) || (display == NULL)){
	return BadValue;
    }
    if(subpicture->privData == NULL) {
	return (error_base + XvMCBadSubpicture);
    }
    pViaSubPic = (ViaXvMCSubPicture *) subpicture->privData;
    for (i=0; i < VIA_SUBPIC_PALETTE_SIZE; ++i) {
	tmp = *palette++ << 8;
	tmp |= *palette++ << 16;
	tmp |= *palette++ << 24;
	tmp |= ((i & 0x0f) << 4) | 0x07;
	pViaSubPic->palette[i] = tmp;
    }

    pViaXvMC = pViaSubPic->privContext;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    sAPriv = SAREAPTR( pViaXvMC );
    hwlLock(&pViaXvMC->xl,1);
    setLowLevelLocking(&pViaXvMC->xl,0);

    /*
     * If the subpicture is displaying, Immeadiately update it with the
     * new palette.
     */

    if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] == 
	(pViaSubPic->srfNo | VIA_XVMC_VALID)) {
        viaVideoSubPictureLocked(&pViaXvMC->xl,pViaSubPic);
    }
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    setLowLevelLocking(&pViaXvMC->xl,1);
    hwlUnlock(&pViaXvMC->xl,1);
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}


static int findOverlap(unsigned width,unsigned height,
		       short *dstX, short *dstY,
		       short *srcX, short *srcY,
		       unsigned short *areaW, unsigned short *areaH) 
{
    int
	w,h;
    unsigned
	mWidth,mHeight;
    
    w = *areaW;
    h = *areaH;

    if ((*dstX >= width) || (*dstY >= height)) 
	return 1;
    if (*dstX < 0) {
	w += *dstX;
	*srcX -= *dstX;
	*dstX = 0;
    }
    if (*dstY < 0) {
	h += *dstY;
	*srcY -= *dstY;
	*dstY = 0;
    }
    if ((w <= 0) || ((h <= 0))) 
	return 1;
    mWidth = width - *dstX;
    mHeight = height - *dstY;
    *areaW = (w <= mWidth) ? w : mWidth;
    *areaH = (h <= mHeight) ? h : mHeight; 
    return 0;
}
    


Status XvMCClearSubpicture (
    Display *display,
    XvMCSubpicture *subpicture,
    short x,
    short y,
    unsigned short width,
    unsigned short height,
    unsigned int color
    ) 
{

    ViaXvMCContext *pViaXvMC;
    ViaXvMCSubPicture *pViaSubPic;
    short dummyX,dummyY;
    unsigned long bOffs;

    if((subpicture == NULL) || (display == NULL)) {
	return BadValue;
    }
    if(subpicture->privData == NULL) {
	return (error_base + XvMCBadSubpicture);
    }
    pViaSubPic = (ViaXvMCSubPicture *) subpicture->privData;
    pViaXvMC = pViaSubPic->privContext;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    /* Clip clearing area so that it fits inside subpicture. */

    if (findOverlap(subpicture->width, subpicture->height, &x, &y,
		    &dummyX, &dummyY, &width, &height)) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return Success;
    }
		    
    bOffs = pViaSubPic->offset + y*pViaSubPic->stride + x;
    viaBlit(&pViaXvMC->xl, 8, 0, pViaSubPic->stride, bOffs, pViaSubPic->stride,
	    width, height, 1, 1, VIABLIT_FILL, color);
    pViaSubPic->needsSync = 1;
    pViaSubPic->timeStamp = viaDMATimeStampLowLevel(&pViaXvMC->xl);
    if (flushXvMCLowLevel(&pViaXvMC->xl)) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadValue;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}

Status
XvMCCompositeSubpicture (
    Display *display,
    XvMCSubpicture *subpicture,
    XvImage *image,
    short srcx,
    short srcy,
    unsigned short width,
    unsigned short height,
    short dstx,
    short dsty
    ) 
{
    
    unsigned i;
    ViaXvMCContext *pViaXvMC;
    ViaXvMCSubPicture *pViaSubPic;
    CARD8 *dAddr, *sAddr;

    if((subpicture == NULL) || (display == NULL) || (image == NULL)){
	return BadValue;
    }
    if(NULL == (pViaSubPic = (ViaXvMCSubPicture *)subpicture->privData)) {
	return (error_base + XvMCBadSubpicture);
    }

    pViaXvMC = pViaSubPic->privContext;


    if (image->id != subpicture->xvimage_id) 
	return BadMatch;

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    

    /*
     * Clip copy area so that it fits inside subpicture and image.
     */

    if (findOverlap(subpicture->width, subpicture->height, 
		    &dstx, &dsty, &srcx, &srcy, &width, &height)) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return Success;
    }
    if (findOverlap(image->width, image->height, 
		    &srcx, &srcy, &dstx, &dsty, &width, &height)) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return Success;
    }

    if (pViaSubPic->needsSync) {
	if (syncXvMCLowLevel(&pViaXvMC->xl, LL_MODE_2D, 0, pViaSubPic->timeStamp)) {
	    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	    return BadValue;
	}
	pViaSubPic->needsSync = 0;
    }

    for(i=0; i<height; ++i) {
        dAddr = (((CARD8 *)pViaXvMC->fbAddress) + 
	         (pViaSubPic->offset + (dsty+i)*pViaSubPic->stride + dstx));
        sAddr = (((CARD8 *)image->data) + 
		 (image->offsets[0] + (srcy+i)*image->pitches[0] + srcx));
	memcpy(dAddr,sAddr,width);
    }

    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}



Status
XvMCBlendSubpicture (
    Display *display,
    XvMCSurface *target_surface,
    XvMCSubpicture *subpicture,
    short subx,
    short suby,
    unsigned short subw,
    unsigned short subh,
    short surfx,
    short surfy,
    unsigned short surfw,
    unsigned short surfh
    ) 
{    
    ViaXvMCSurface *pViaSurface;
    ViaXvMCSubPicture *pViaSubPic;

    if((display == NULL) || target_surface == NULL){
	return BadValue;
    }
    
    if (subx || suby || surfx || surfy ||
	(subw != surfw) || (subh != surfh))  {
	fprintf(stderr,"ViaXvMC: Only completely overlapping subpicture "
		"supported.\n");
	return BadValue;
    }

    if(NULL == (pViaSurface = target_surface->privData)) {
	return (error_base + XvMCBadSurface);
    }

    if (subpicture) {

	if(NULL == (pViaSubPic = subpicture->privData)) {
	    return (error_base + XvMCBadSubpicture);
	}
	
	pViaSurface->privSubPic = pViaSubPic;
    } else {
	pViaSurface->privSubPic = NULL;
    }
    return Success;
}

Status
XvMCBlendSubpicture2 (
    Display *display,
    XvMCSurface *source_surface,
    XvMCSurface *target_surface,
    XvMCSubpicture *subpicture,
    short subx,
    short suby,
    unsigned short subw,
    unsigned short subh,
    short surfx,
    short surfy,
    unsigned short surfw,
    unsigned short surfh
    ) 
{    
    ViaXvMCSurface *pViaSurface,*pViaSSurface;
    ViaXvMCSubPicture *pViaSubPic;
    ViaXvMCContext *pViaXvMC;

    unsigned width,height;

    if((display == NULL) || target_surface == NULL || source_surface == NULL){
	return BadValue;
    }
    
    if (subx || suby || surfx || surfy ||
	(subw != surfw) || (subh != surfh))  {
	fprintf(stderr,"ViaXvMC: Only completely overlapping subpicture "
		"supported.\n");
	return BadMatch;
    }

    if(NULL == (pViaSurface = target_surface->privData)) {
	return (error_base + XvMCBadSurface);
    }

    if(NULL == (pViaSSurface = source_surface->privData)) {
	return (error_base + XvMCBadSurface);
    }
    pViaXvMC = pViaSurface->privContext;
    width = pViaSSurface->width;
    height = pViaSSurface->height;
    if (width != pViaSurface->width || height != pViaSSurface->height) {
	return BadMatch;
    }

    if (XvMCSyncSurface(display,source_surface)) {
        return BadValue;
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    viaBlit(&pViaXvMC->xl, 8, yOffs(pViaSSurface), pViaSSurface->yStride,
	    yOffs(pViaSurface), pViaSurface->yStride,
	    width, height, 1, 1, VIABLIT_COPY, 0);
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    viaBlit(&pViaXvMC->xl, 8, uOffs(pViaSSurface), pViaSSurface->yStride >> 1,
	    uOffs(pViaSurface), pViaSurface->yStride >> 1,
	    width >> 1, height >> 1, 1, 1, VIABLIT_COPY, 0);
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    viaBlit(&pViaXvMC->xl, 8, vOffs(pViaSSurface), pViaSSurface->yStride >> 1,
	    vOffs(pViaSurface), pViaSurface->yStride >> 1,
	    width >> 1, height >> 1, 1, 1, VIABLIT_COPY, 0);
    pViaSurface->needsSync = 1;
    pViaSurface->syncMode = LL_MODE_2D;
    pViaSurface->timeStamp = viaDMATimeStampLowLevel(&pViaXvMC->xl);
    if (flushXvMCLowLevel(&pViaXvMC->xl)) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadValue;
    }
    if (subpicture) {

	if(NULL == (pViaSubPic = subpicture->privData)) {
	    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	    return (error_base + XvMCBadSubpicture);
	}
	
	pViaSurface->privSubPic = pViaSubPic;
    } else {
	pViaSurface->privSubPic = NULL;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return Success;
}


Status
XvMCSyncSubpicture (Display *display, XvMCSubpicture *subpicture) 
{
    ViaXvMCSubPicture *pViaSubPic;
    ViaXvMCContext *pViaXvMC;
    Status retVal=0;

    if((display == NULL) || subpicture == NULL){
	return BadValue;
    }
    if(NULL == (pViaSubPic = subpicture->privData)) {
	return (error_base + XvMCBadSubpicture);
    }

    pViaXvMC = pViaSubPic->privContext;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (pViaSubPic->needsSync) {
	if (syncXvMCLowLevel(&pViaXvMC->xl, LL_MODE_2D, 
			     0, pViaSubPic->timeStamp)) {
	    retVal = BadValue;
	}
	pViaSubPic->needsSync = 0;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    return retVal;
}

Status
XvMCFlushSubpicture (Display *display, XvMCSubpicture *subpicture) 
{
    ViaXvMCSubPicture *pViaSubPic;

    if((display == NULL) || subpicture == NULL){
	return BadValue;
    }
    if(NULL == (pViaSubPic = subpicture->privData)) {
	return (error_base + XvMCBadSubpicture);
    }
    
    return Success;
}

Status
XvMCDestroySubpicture (Display *display, XvMCSubpicture *subpicture) 
{
    ViaXvMCSubPicture *pViaSubPic;
    ViaXvMCContext *pViaXvMC;
    volatile ViaXvMCSAreaPriv *sAPriv;

    if((display == NULL) || subpicture == NULL){
	return BadValue;
    }
    if(NULL == (pViaSubPic = subpicture->privData)) {
	return (error_base + XvMCBadSubpicture);
    }
    pViaXvMC = pViaSubPic->privContext;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    
    sAPriv = SAREAPTR(pViaXvMC);
    hwlLock(&pViaXvMC->xl,1); 
    setLowLevelLocking(&pViaXvMC->xl,0);
    if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] == 
	( pViaSubPic->srfNo | VIA_XVMC_VALID )) {
        viaVideoSubPictureOffLocked(&pViaXvMC->xl);
	sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] = 0;
    }
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    setLowLevelLocking(&pViaXvMC->xl,1);
    hwlUnlock(&pViaXvMC->xl,1);

    XLockDisplay(display);
    _xvmc_destroy_subpicture(display,subpicture);
    XUnlockDisplay(display);

    free(pViaSubPic);
    subpicture->privData = NULL;
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );

    return Success;
}

Status
XvMCGetSubpictureStatus (Display *display, XvMCSubpicture *subpic, int *stat) 
{
    ViaXvMCSubPicture *pViaSubPic;
    ViaXvMCContext *pViaXvMC;
    volatile ViaXvMCSAreaPriv *sAPriv;
   

    if((display == NULL) || subpic == NULL){
	return BadValue;
    }
    if(NULL == (pViaSubPic = subpic->privData)) {
	return (error_base + XvMCBadSubpicture);
    }
    if (stat) {
	*stat = 0;
	pViaXvMC = pViaSubPic->privContext;
	sAPriv = SAREAPTR( pViaXvMC );
	if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] == 
	    (pViaSubPic->srfNo | VIA_XVMC_VALID)) 
	    *stat |= XVMC_DISPLAYING;	    
    }
    return Success;
}

Status
XvMCFlushSurface (Display *display, XvMCSurface *surface)
{
    ViaXvMCSurface *pViaSurface;
    ViaXvMCContext *pViaXvMC;
    Status ret;

    if((display == NULL) || surface == NULL){
	return BadValue;
    }
    if(NULL == (pViaSurface = surface->privData)) {
	return (error_base + XvMCBadSurface);
    }

    pViaXvMC = pViaSurface->privContext;
    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (pViaSurface->needsSync) 
	pViaSurface->timeStamp = pViaXvMC->timeStamp = 
	  viaDMATimeStampLowLevel(&pViaXvMC->xl);
    ret = (flushXvMCLowLevel(&pViaXvMC->xl)) ? BadValue : Success;
    if (pViaXvMC->rendSurf[0] == (pViaSurface->srfNo | VIA_XVMC_VALID)) {
	hwlLock(&pViaXvMC->xl,0);
	pViaXvMC->haveDecoder = 0;
	releaseDecoder(pViaXvMC, 0);
	hwlUnlock(&pViaXvMC->xl,0);
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );
    sched_yield();
    return ret;
}

Status
XvMCGetSurfaceStatus (Display *display, XvMCSurface *surface, int *stat) 
{
    ViaXvMCSurface *pViaSurface;
    ViaXvMCContext *pViaXvMC;
    volatile ViaXvMCSAreaPriv *sAPriv;
    unsigned i;
    int ret = 0;

    if((display == NULL) || surface == NULL){
	return BadValue;
    }
    if(NULL == (pViaSurface = surface->privData)) {
	return (error_base + XvMCBadSurface);
    }
    if (stat) {
	*stat = 0;
	pViaXvMC = pViaSurface->privContext;
	pthread_mutex_lock( &pViaXvMC->ctxMutex ); 
	sAPriv = SAREAPTR( pViaXvMC );
	if (sAPriv->XvMCDisplaying[pViaXvMC->xvMCPort] 
	    == (pViaSurface->srfNo | VIA_XVMC_VALID)) 
	    *stat |= XVMC_DISPLAYING;
	for (i=0; i<VIA_MAX_RENDSURF; ++i) {
	    if(pViaXvMC->rendSurf[i] == 
	       (pViaSurface->srfNo | VIA_XVMC_VALID)) {
		*stat |= XVMC_RENDERING;
		break;
	    }
	}
	pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
    }
    return ret;
}

XvAttribute *
XvMCQueryAttributes (
    Display *display,
    XvMCContext *context,
    int *number
    )
{
    ViaXvMCContext *pViaXvMC;
    XvAttribute *ret;
    unsigned long siz;

    *number = 0;
    if ((display == NULL) || (context == NULL)) {
	return NULL;
    }
    
    if (NULL == (pViaXvMC = context->privData)) {
	return NULL;
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );
    if (NULL != (ret = (XvAttribute *)
		 malloc(siz = VIA_NUM_XVMC_ATTRIBUTES*sizeof(XvAttribute)))) {
	memcpy(ret,pViaXvMC->attribDesc,siz);
	*number =  VIA_NUM_XVMC_ATTRIBUTES;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex );

    return ret;    
}

Status
XvMCSetAttribute (
    Display *display,
    XvMCContext *context, 
    Atom attribute, 
    int value
    )
{
    int found;
    unsigned i;
    ViaXvMCContext *pViaXvMC;
    ViaXvMCCommandBuffer buf;
    
    if ((display == NULL) || (context == NULL)) {
	return (error_base + XvMCBadContext);
    }
    
    if (NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex );

    found = 0;
    for (i=0; i < pViaXvMC->attrib.numAttr; ++i) {
	if (attribute == pViaXvMC->attrib.attributes[i].attribute) {
	    if ((!(pViaXvMC->attribDesc[i].flags & XvSettable)) ||
		value < pViaXvMC->attribDesc[i].min_value ||
		value > pViaXvMC->attribDesc[i].max_value) 
		return BadValue;
	    pViaXvMC->attrib.attributes[i].value = value;
	    found = 1;
	    pViaXvMC->attribChanged = 1;
	    break;
	}
    }
    if (!found) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex );
	return BadMatch;
    }
    if (pViaXvMC->haveXv) {
	buf.command = VIA_XVMC_COMMAND_ATTRIBUTES;
	pViaXvMC->xvImage->data = (char *)&buf;
	buf.ctxNo = pViaXvMC->ctxNo | VIA_XVMC_VALID;
	buf.attrib = pViaXvMC->attrib;
	XLockDisplay(display);
	pViaXvMC->attribChanged = 
	    XvPutImage(display,pViaXvMC->port,pViaXvMC->draw,
		       pViaXvMC->gc,
		       pViaXvMC->xvImage,0,0,1,1,0,0,1,1);
	XUnlockDisplay(display);
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
    return Success;
}
    

Status
XvMCGetAttribute (
    Display *display,
    XvMCContext *context, 
    Atom attribute, 
    int *value
    )
{
    int found;
    unsigned i;
    ViaXvMCContext *pViaXvMC;
    
    if ((display == NULL) || (context == NULL)) {
	return (error_base + XvMCBadContext);
    }
    
    if (NULL == (pViaXvMC = context->privData)) {
	return (error_base + XvMCBadContext);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex ); 
    found = 0;
    for (i=0; i < pViaXvMC->attrib.numAttr; ++i) {
	if (attribute == pViaXvMC->attrib.attributes[i].attribute) {
	    if (pViaXvMC->attribDesc[i].flags & XvGettable) {
		*value = pViaXvMC->attrib.attributes[i].value;
		found = 1;
		break;
	    }
	}
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 

    if (!found) 
	return BadMatch;
    return Success;
}
    

Status XvMCHideSurface(Display *display,XvMCSurface *surface)
{

    ViaXvMCSurface *pViaSurface;
    ViaXvMCContext *pViaXvMC;
    ViaXvMCSubPicture *pViaSubPic;
    volatile ViaXvMCSAreaPriv *sAPriv;
    ViaXvMCCommandBuffer buf;
    Status ret;

    if ((display == NULL) || (surface == NULL)) {
	return BadValue;
    }
    if (NULL == (pViaSurface = surface->privData )) {
	return (error_base + XvMCBadSurface);
    }
    if (NULL == (pViaXvMC = pViaSurface->privContext)) {
	return (error_base + XvMCBadContext);
    }

    pthread_mutex_lock( &pViaXvMC->ctxMutex ); 
    if (!pViaXvMC->haveXv) {
	pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
	return Success;
    }
    
    sAPriv = SAREAPTR( pViaXvMC );
    hwlLock(&pViaXvMC->xl,1); 

    if (sAPriv->XvMCDisplaying[pViaXvMC->xvMCPort] != 
	(pViaSurface->srfNo | VIA_XVMC_VALID)) {
	hwlUnlock(&pViaXvMC->xl,1);
	pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
	return Success;
    }
    setLowLevelLocking(&pViaXvMC->xl,0);
    if (NULL != (pViaSubPic = pViaSurface->privSubPic)) {
	if (sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] ==
	    (pViaSubPic->srfNo | VIA_XVMC_VALID)) {
	    sAPriv->XvMCSubPicOn[pViaXvMC->xvMCPort] &= ~VIA_XVMC_VALID;
	    viaVideoSubPictureOffLocked(&pViaXvMC->xl); 
	}
    }
    flushPCIXvMCLowLevel(&pViaXvMC->xl);
    setLowLevelLocking(&pViaXvMC->xl,1);
    hwlUnlock(&pViaXvMC->xl,1);
	    
    buf.command = VIA_XVMC_COMMAND_UNDISPLAY;
    buf.ctxNo = pViaXvMC->ctxNo | VIA_XVMC_VALID;
    buf.srfNo = pViaSurface->srfNo | VIA_XVMC_VALID;
    pViaXvMC->xvImage->data = (char *)&buf;
    if ((ret = XvPutImage(display,pViaXvMC->port,pViaXvMC->draw,
			  pViaXvMC->gc,
			  pViaXvMC->xvImage,0,0,1,1,0,0,1,1))) {
	fprintf(stderr,"XvMCPutSurface: Hiding overlay failed.\n");
	pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
	return ret;
    }
    pthread_mutex_unlock( &pViaXvMC->ctxMutex ); 
    return Success;
}
