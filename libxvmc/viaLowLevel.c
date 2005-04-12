/*****************************************************************************
 * VIA Unichrome XvMC extension client lib.
 *
 * Copyright (c) 2004 Thomas Hellström. All rights reserved.
 * Copyright (c) 2003 Andreas Robinson. All rights reserved.
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
 * Low-level functions that deal directly with the hardware. In the future,
 * these functions might be implemented in a kernel module. Also, some of them
 * would benefit from DMA.
 *
 * Authors: Andreas Robinson 2003. Thomas Hellström 2004. Ivor Hewitt 2005.
 */

#define VIDEO_DMA
/*#define HQV_USE_IRQ */

#include "viaXvMCPriv.h"
#include "viaLowLevel.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#define PCI_CHIP_VT3204         0x3108 /* K8M800 */
#define PCI_CHIP_VT3259         0x3118 /* PM800/PM880/CN400 */
#define PCI_CHIP_CLE3122        0x3122 /* CLE266 */
#define PCI_CHIP_VT3205         0x7205 /* KM400 */

/*
 * For Other architectures than i386 these might have to be modified for
 * bigendian etc.
 */

#define MPEGIN(xl,reg)							\
    *((volatile CARD32 *)(((CARD8 *)(xl)->mmioAddress) + 0xc00 + (reg)))

#define VIDIN(ctx,reg)							\
    *((volatile CARD32 *)(((CARD8 *)(ctx)->mmioAddress) + 0x200 + (reg)))

#define REGIN(ctx,reg)							\
    *((volatile CARD32 *)(((CARD8 *)(ctx)->mmioAddress) + 0x0000 + (reg)))

#define HQV_CONTROL             0x1D0
#define HQV_SRC_OFFSET          0x1CC
#define HQV_SRC_STARTADDR_Y     0x1D4
#define HQV_SRC_STARTADDR_U     0x1D8
#define HQV_SRC_STARTADDR_V     0x1DC
#define HQV_MINIFY_DEBLOCK      0x1E8    

#define REG_HQV1_INDEX      0x00001000
    
#define HQV_SW_FLIP         0x00000010
#define HQV_FLIP_STATUS     0x00000001
#define HQV_SUBPIC_FLIP     0x00008000
#define HQV_FLIP_ODD        0x00000020
#define HQV_DEINTERLACE     0x00010000
#define HQV_FIELD_2_FRAME   0x00020000
#define HQV_FRAME_2_FIELD   0x00040000
#define HQV_FIELD_UV        0x00100000
#define HQV_DEBLOCK_HOR     0x00008000
#define HQV_DEBLOCK_VER     0x80000000
#define HQV_YUV420          0xC0000000
#define HQV_YUV422          0x80000000
#define HQV_ENABLE          0x08000000
#define HQV_GEN_IRQ         0x00000080

#define V_COMPOSE_MODE          0x98
#define V1_COMMAND_FIRE         0x80000000  
#define V3_COMMAND_FIRE         0x40000000  

/* SUBPICTURE Registers */
#define SUBP_CONTROL_STRIDE     0x1C0
#define SUBP_STARTADDR          0x1C4
#define RAM_TABLE_CONTROL       0x1C8
#define RAM_TABLE_READ          0x1CC

/* SUBP_CONTROL_STRIDE              0x3c0 */
#define SUBP_HQV_ENABLE             0x00010000
#define SUBP_IA44                   0x00020000
#define SUBP_AI44                   0x00000000
#define SUBP_STRIDE_MASK            0x00001fff
#define SUBP_CONTROL_MASK           0x00070000

/* RAM_TABLE_CONTROL                0x3c8 */
#define RAM_TABLE_RGB_ENABLE        0x00000007


#define VIA_REG_STATUS          0x400
#define VIA_REG_GEMODE          0x004
#define VIA_REG_SRCBASE         0x030
#define VIA_REG_DSTBASE         0x034
#define VIA_REG_PITCH           0x038      
#define VIA_REG_SRCCOLORKEY     0x01C      
#define VIA_REG_KEYCONTROL      0x02C       
#define VIA_REG_SRCPOS          0x008
#define VIA_REG_DSTPOS          0x00C
#define VIA_REG_GECMD           0x000
#define VIA_REG_DIMENSION       0x010       /* width and height */
#define VIA_REG_FGCOLOR         0x018


#define VIA_VR_QUEUE_BUSY       0x00020000 /* Virtual Queue is busy */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_GEM_8bpp            0x00000000
#define VIA_GEM_16bpp           0x00000100
#define VIA_GEM_32bpp           0x00000300
#define VIA_GEC_BLT             0x00000001
#define VIA_PITCH_ENABLE        0x80000000
#define VIA_GEC_INCX            0x00000000
#define VIA_GEC_DECY            0x00004000
#define VIA_GEC_INCY            0x00000000
#define VIA_GEC_DECX            0x00008000
#define VIA_GEC_FIXCOLOR_PAT    0x00002000


#define VIA_BLIT_CLEAR 0x00
#define VIA_BLIT_COPY 0xCC
#define VIA_BLIT_FILL 0xF0
#define VIA_BLIT_SET 0xFF

#define VIA_SYNCWAITTIMEOUT 50000 /* Might be a bit conservative */
#define VIA_DMAWAITTIMEOUT 150000
#define VIA_VIDWAITTIMEOUT 50000
#define VIA_XVMC_DECODERTIMEOUT 50000 /*(microseconds)*/

#define VIA_AGP_HEADER5 0xFE040000
#define VIA_AGP_HEADER6 0xFE050000

#define H1_ADDR(val) (((val) >> 2) | 0xF0000000)
#define WAITFLAGS(xl, flags)			\
    (xl)->curWaitFlags |= (flags)
#define BEGIN_RING_AGP(xl,size)						\
    do {								\
	if ((xl)->agp_pos > (LL_AGP_CMDBUF_SIZE-(size))) {		\
	    agpFlush(xl);						\
	}								\
    } while(0)
#define OUT_RING_AGP(xl, val) do{			\
	(xl)->agp_buffer[(xl)->agp_pos++] = (val);	\
  } while(0);		

#define OUT_RING_QW_AGP(xl, val1, val2)			\
    do {						\
	(xl)->agp_buffer[(xl)->agp_pos++] = (val1);	\
	(xl)->agp_buffer[(xl)->agp_pos++] = (val2);	\
    } while (0)
  

#define BEGIN_HEADER5_AGP(xl, index)		\
    do {					\
	BEGIN_RING_AGP(xl, 8);			\
	(xl)->agp_mode = VIA_AGP_HEADER5;	\
        (xl)->agp_index = (index);              \
	(xl)->agp_header_start = (xl)->agp_pos; \
	(xl)->agp_pos += 4;			\
    } while (0)

#define BEGIN_HEADER6_AGP(xl)			\
    do {					\
	BEGIN_RING_AGP(xl, 8);			\
	(xl)->agp_mode = VIA_AGP_HEADER6;	\
	(xl)->agp_header_start = (xl)->agp_pos; \
	(xl)->agp_pos += 4;			\
    } while (0)

#define BEGIN_HEADER5_DATA(xl, size, index)				\
    do {								\
	if ((xl)->agp_pos > (LL_AGP_CMDBUF_SIZE-((size) + 16))) {	\
	    agpFlush(xl);						\
	    BEGIN_HEADER5_AGP((xl), (index));				\
	} else if ((xl)->agp_mode && (((xl)->agp_mode != VIA_AGP_HEADER5) || \
				      (xl)->agp_index != index)) {	\
	    finish_header_agp(xl);					\
	    BEGIN_HEADER5_AGP((xl), (index));				\
	}								\
	else if ((xl->agp_mode != VIA_AGP_HEADER5)) {			\
	    BEGIN_HEADER5_AGP((xl), (index));				\
	}								\
    }while(0)

#define BEGIN_HEADER6_DATA(xl, size)					\
    do{									\
	if ((xl)->agp_pos > (LL_AGP_CMDBUF_SIZE-(((size) << 1) + 16))) {	\
	    agpFlush(xl);						\
	    BEGIN_HEADER6_AGP((xl));					\
	} else	if ((xl)->agp_mode && ((xl)->agp_mode != VIA_AGP_HEADER6)) { \
	    finish_header_agp(xl);					\
	    BEGIN_HEADER6_AGP(xl);					\
	}								\
	else if ((xl->agp_mode != VIA_AGP_HEADER6)) {			\
	    BEGIN_HEADER6_AGP((xl));					\
	}								\
    }while(0)


static void
finish_header_agp(XvMCLowLevel *xl) 
{
    int
      numDWords,i;
    CARD32
	*hb;

    if (!xl->agp_mode) return;
    numDWords = xl->agp_pos - xl->agp_header_start - 4; 
    hb = xl->agp_buffer + xl->agp_header_start;
    switch (xl->agp_mode) {
    case VIA_AGP_HEADER5:
	hb[0] = VIA_AGP_HEADER5 | xl->agp_index;
	hb[1] = numDWords ;
	hb[2] = 0x00F50000; /* SW debug flag. (?) */
	break;
    default:
	hb[0] = VIA_AGP_HEADER6;
	hb[1] = numDWords >> 1 ;
	hb[2] = 0x00F60000; /* SW debug flag. (?) */
	break;
    }
    hb[3] = 0;
    if (numDWords & 3) {
	for (i=0; i<(4 - (numDWords & 3)); ++i) 
	    OUT_RING_AGP(xl, 0x00000000);
    }
    xl->agp_mode = 0;
}

void 
hwlLock(XvMCLowLevel *xl, int videoLock) 
{
    LL_HW_LOCK(xl);
}

void 
hwlUnlock(XvMCLowLevel *xl, int videoLock) 
{
    LL_HW_UNLOCK(xl);
}
    
static unsigned 
timeDiff(struct timeval *now,struct timeval *then) {
    return (now->tv_usec >= then->tv_usec) ?
	now->tv_usec - then->tv_usec : 
	1000000 - (then->tv_usec - now->tv_usec);
}

void 
setAGPSyncLowLevel(XvMCLowLevel *xl, int val, CARD32 timeStamp)
{
    xl->agpSync = val;
    xl->agpSyncTimeStamp = timeStamp;
}

CARD32 
viaDMATimeStampLowLevel(XvMCLowLevel *xl)
{
    if (xl->use_agp) {
	viaBlit(xl, 32, xl->tsOffset, 1, xl->tsOffset, 1, 1, 1, 0, 0, 
		VIABLIT_FILL, xl->curTimeStamp);
	return xl->curTimeStamp++;
    }
    return 0;
}

static void 
viaDMAWaitTimeStamp(XvMCLowLevel *xl, CARD32 timeStamp, int doSleep) 
{
    struct timeval now, then;
    struct timezone here;
    struct timespec sleep, rem;

    if (xl->use_agp && (xl->lastReadTimeStamp - timeStamp > (1 << 23))) {
	sleep.tv_nsec = 1;
	sleep.tv_sec = 0;
	here.tz_minuteswest = 0;
	here.tz_dsttime = 0;
	gettimeofday(&then,&here);
 
	while(((xl->lastReadTimeStamp = *xl->tsP) - timeStamp) > (1 << 23)) {
	    gettimeofday(&now,&here); 
	    if (timeDiff(&now,&then) > VIA_DMAWAITTIMEOUT) {
		if(((xl->lastReadTimeStamp = *xl->tsP) - timeStamp) > (1 << 23)) {
		    xl->errors |= LL_DMA_TIMEDOUT;
		    break;
		}
	    }
	    if (doSleep) nanosleep(&sleep, &rem);
	}
    }
}

static int 
viaDMAInitTimeStamp(XvMCLowLevel *xl) 
{
    int ret = 0;

    if (xl->use_agp) {
	xl->tsMem.context = *(xl->drmcontext);
	xl->tsMem.size = 64;
	xl->tsMem.type = VIDEO;
	if (drmCommandWriteRead(xl->fd, DRM_VIA_ALLOCMEM, &xl->tsMem, sizeof(xl->tsMem)) < 0) 
	    return ret;
	if (xl->tsMem.size != 64)
	    return -1;
	xl->tsOffset = (xl->tsMem.offset + 31) & ~31; 
	xl->tsP = (CARD32 *)xl->fbAddress + (xl->tsOffset >> 2); 
	xl->curTimeStamp = 1;
	*xl->tsP = 0;
    }
    return 0;
}

static int 
viaDMACleanupTimeStamp(XvMCLowLevel *xl) 
{

    if (!(xl->tsMem.size) || !xl->use_agp) return 0;
    return drmCommandWrite(xl->fd, DRM_VIA_FREEMEM, &xl->tsMem, sizeof(xl->tsMem)); 
}


static CARD32 
viaMpegGetStatus(XvMCLowLevel *xl)
{
    return MPEGIN(xl,0x54);
}

static int 
viaMpegIsBusy(XvMCLowLevel *xl, CARD32 mask, CARD32 idle)
{
    CARD32 tmp = viaMpegGetStatus(xl);

    /*
     * Error detected. 
     * FIXME: Are errors really shown when error concealment is on?
     */

    if (tmp & 0x70) return 0; 

    return (tmp & mask) != idle;
}


static void 
syncDMA(XvMCLowLevel *xl, unsigned int doSleep)
{
  
    /*
     * Ideally, we'd like to have an interrupt wait here, but, according to second hand
     * information, the hardware does not support this, although earlier S3 chips do that.
     * It is therefore not implemented into the DRM, and we'll do a user space wait here.
     */

    struct timeval now, then;
    struct timezone here;
    struct timespec sleep, rem;

   
    sleep.tv_nsec = 1;
    sleep.tv_sec = 0;
    here.tz_minuteswest = 0;
    here.tz_dsttime = 0;
    gettimeofday(&then,&here);
    while( !(REGIN(xl, VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY)) {
	gettimeofday(&now,&here); 
	if (timeDiff(&now,&then) > VIA_DMAWAITTIMEOUT) {
	    if( !(REGIN(xl, VIA_REG_STATUS) & VIA_VR_QUEUE_BUSY)) {
		xl->errors |= LL_DMA_TIMEDOUT;
		break;
	    }
	}
	if (doSleep) nanosleep(&sleep, &rem);
    }
    while( REGIN(xl, VIA_REG_STATUS) & VIA_CMD_RGTR_BUSY ) {
	gettimeofday(&now,&here); 
	if (timeDiff(&now,&then) > VIA_DMAWAITTIMEOUT) {
	    if( REGIN(xl, VIA_REG_STATUS) & VIA_CMD_RGTR_BUSY ) {
		xl->errors |= LL_DMA_TIMEDOUT;
		break;
	    }
	}
	if (doSleep) nanosleep(&sleep, &rem);
    }
}

#ifdef HQV_USE_IRQ
static void 
syncVideo(XvMCLowLevel *xl, unsigned int doSleep)
{
    int proReg=0;
    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;

    /*
     * Wait for HQV completion using completion interrupt. Nothing strange here. 
     * Note that the interrupt handler clears the HQV_FLIP_STATUS bit, so we 
     * can't wait on that one.
     */
    
  if ((VIDIN(xl, HQV_CONTROL|proReg) & (HQV_SW_FLIP | HQV_SUBPIC_FLIP))) {
	drm_via_irqwait_t irqw;
	irqw.request.irq = 1;
	irqw.request.type = VIA_IRQ_ABSOLUTE;
	if (drmCommandWriteRead(xl->fd, DRM_VIA_WAIT_IRQ, &irqw, sizeof(irqw)) < 0)
		xl->errors |= LL_VIDEO_TIMEDOUT;
    }
}
#else
static void 
syncVideo(XvMCLowLevel *xl, unsigned int doSleep)
{
    /*
     * Wait for HQV completion. Nothing strange here. We assume that the HQV
     * Handles syncing to the V1 / V3 engines by itself. It should be safe to
     * always wait for SUBPIC_FLIP completion although subpictures are not always
     * used. 
     */
    
    struct timeval now, then;
    struct timezone here;
    struct timespec sleep, rem;

    int proReg=0;
    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;

    sleep.tv_nsec = 1;
    sleep.tv_sec = 0;
    here.tz_minuteswest = 0;
    here.tz_dsttime = 0;
    gettimeofday(&then,&here); 
    while((VIDIN(xl, HQV_CONTROL|proReg) & (HQV_SW_FLIP | HQV_SUBPIC_FLIP  )) ) {
	gettimeofday(&now,&here); 
	if (timeDiff(&now,&then) > VIA_SYNCWAITTIMEOUT) {
	  if((VIDIN(xl, HQV_CONTROL|proReg) & (HQV_SW_FLIP | HQV_SUBPIC_FLIP )) ) {
		xl->errors |= LL_VIDEO_TIMEDOUT;
		break;
	    }
	}
	if (doSleep) nanosleep(&sleep, &rem);
    }
}
#endif

static void 
syncAccel(XvMCLowLevel *xl, unsigned int mode, unsigned int doSleep)
{
    struct timeval now, then;
    struct timezone here;
    struct timespec sleep, rem;
    CARD32 mask = ((mode & LL_MODE_2D) ? VIA_2D_ENG_BUSY : 0) |
	((mode & LL_MODE_3D) ? VIA_3D_ENG_BUSY : 0);

    sleep.tv_nsec = 1;
    sleep.tv_sec = 0;
    here.tz_minuteswest = 0;
    here.tz_dsttime = 0;
    gettimeofday(&then,&here); 
    while( REGIN(xl, VIA_REG_STATUS) & mask) {
	gettimeofday(&now,&here); 
	if (timeDiff(&now,&then) > VIA_SYNCWAITTIMEOUT) {
	    if( REGIN(xl, VIA_REG_STATUS) & mask) {
		xl->errors |= LL_ACCEL_TIMEDOUT;
		break;
	    }
	}
	if (doSleep) nanosleep(&sleep, &rem);
    }
}


static void 
syncMpeg(XvMCLowLevel *xl, unsigned int mode, unsigned int doSleep)
{
    /*
     * Ideally, we'd like to have an interrupt wait here, but from information from VIA
     * at least the MPEG completion interrupt is broken on the CLE266, which was
     * discovered during validation of the chip.
     */

    struct timeval now, then;
    struct timezone here;
    struct timespec sleep, rem;
    CARD32 busyMask = 0;
    CARD32 idleVal = 0;
    CARD32 ret;

    sleep.tv_nsec = 1;
    sleep.tv_sec = 0;
    here.tz_minuteswest = 0;
    here.tz_dsttime = 0;
    gettimeofday(&then,&here); 
    if (mode & LL_MODE_DECODER_SLICE) {
	busyMask = VIA_SLICEBUSYMASK;
	idleVal = VIA_SLICEIDLEVAL;
    }
    if (mode & LL_MODE_DECODER_IDLE) {
	busyMask |= VIA_BUSYMASK;
	idleVal = VIA_IDLEVAL;
    }
    while(viaMpegIsBusy(xl, busyMask, idleVal)) {
	gettimeofday(&now,&here); 
	if (timeDiff(&now,&then) > VIA_XVMC_DECODERTIMEOUT) {
	    if (viaMpegIsBusy(xl, busyMask, idleVal)) {
		xl->errors |= LL_DECODER_TIMEDOUT;
	    }
	    break;
	}
	if (doSleep) nanosleep(&sleep, &rem);
    }
  
    ret = viaMpegGetStatus(xl);
    if (ret & 0x70) {
	xl->errors |= ((ret & 0x70) >> 3);
    }
    return;
}

static void 
pciFlush(XvMCLowLevel *xl)
{
    int ret;
    drm_via_cmdbuffer_t b;
    unsigned mode=xl->curWaitFlags;
  
    finish_header_agp(xl);
    b.buf = (char *)xl->pci_buffer;
    b.size = xl->pci_pos * sizeof(CARD32);
    if (xl->performLocking) hwlLock(xl,0);
    if ((mode != LL_MODE_VIDEO) && (mode != 0)) {
      syncDMA(xl, 0);
    }
    if ((mode & LL_MODE_2D) || (mode & LL_MODE_3D)) {
      syncAccel(xl, mode, 0); 
    }
    if (mode & LL_MODE_VIDEO) {
      syncVideo(xl, 1);
    }
    if (mode & (LL_MODE_DECODER_SLICE | LL_MODE_DECODER_IDLE)) {
      syncMpeg(xl, mode, 0); 
    }
    ret = drmCommandWrite(xl->fd, DRM_VIA_PCICMD, &b, sizeof(b));
    if (xl->performLocking) hwlUnlock(xl,0);
    if (ret) {
	xl->errors |= LL_PCI_COMMAND_ERR;
    }
    xl->pci_pos = 0;
    xl->curWaitFlags = 0;
}
  
static void 
agpFlush(XvMCLowLevel *xl)
{
    drm_via_cmdbuffer_t b;
    int ret;
    int i;

    finish_header_agp(xl);
    if (xl->use_agp) {
	b.buf = (char *)xl->agp_buffer;
	b.size = xl->agp_pos * sizeof(CARD32);
	if (xl->agpSync) {
	    syncXvMCLowLevel(xl, LL_MODE_DECODER_IDLE, 1, xl->agpSyncTimeStamp);
	    xl->agpSync = 0;
	}
	if (xl->performLocking) hwlLock(xl,0);
	do {
	    ret = drmCommandWrite(xl->fd, DRM_VIA_CMDBUFFER, &b, sizeof(b));
	} while (-EAGAIN == ret);
	if (xl->performLocking) hwlUnlock(xl,0);
      
	if (ret) {
	    xl->errors |= LL_AGP_COMMAND_ERR;
	    for(i=0; i<xl->agp_pos; i+=2) {
	      printf("0x%lx, 0x%lx\n", xl->agp_buffer[i], xl->agp_buffer[i+1]);
	    }
	    exit(-1);
	} else {
	    xl->agp_pos = 0;
	}
	xl->curWaitFlags &= LL_MODE_VIDEO;
    } else {
	unsigned mode=xl->curWaitFlags;
  
	b.buf = (char *)xl->agp_buffer;
	b.size = xl->agp_pos * sizeof(CARD32);
	if (xl->performLocking) hwlLock(xl,0);
	if ((mode != LL_MODE_VIDEO) && (mode != 0)) 
	    syncDMA(xl, 0);
	if ((mode & LL_MODE_2D) || (mode & LL_MODE_3D)) 
	    syncAccel(xl, mode, 0);
	if (mode & LL_MODE_VIDEO) 
	    syncVideo(xl, 1);
	if (mode & (LL_MODE_DECODER_SLICE | LL_MODE_DECODER_IDLE))
	    syncMpeg(xl, mode, 0);
	ret = drmCommandWrite(xl->fd, DRM_VIA_PCICMD, &b, sizeof(b));
	if (xl->performLocking) hwlUnlock(xl,0);
	if (ret) {
	    xl->errors |= LL_PCI_COMMAND_ERR;
	}
	xl->agp_pos = 0;
	xl->curWaitFlags = 0;
    }      
}

unsigned 
flushXvMCLowLevel(XvMCLowLevel *xl) 
{
    unsigned 
	errors;

    if(xl->pci_pos) pciFlush(xl);
    if(xl->agp_pos) agpFlush(xl);
    errors = xl->errors;
    if (errors) printf("Error 0x%x\n", errors);
    xl->errors = 0;
    return errors;
}

void 
flushPCIXvMCLowLevel(XvMCLowLevel *xl) 
{
    if(xl->pci_pos) pciFlush(xl); 
    if ((!xl->use_agp &&  xl->agp_pos)) agpFlush(xl);
}


__inline static void pciCommand(XvMCLowLevel *xl, unsigned offset, unsigned value, unsigned flags)
{
    if (xl->pci_pos > (LL_PCI_CMDBUF_SIZE-2)) pciFlush(xl); 
    if (flags) xl->curWaitFlags |= flags;
    xl->pci_buffer[xl->pci_pos++] = /*(offset >> 2) | 0xF0000000;*/ offset;
    xl->pci_buffer[xl->pci_pos++] = value;
}

void 
viaMpegSetSurfaceStride(XvMCLowLevel * xl, ViaXvMCContext *ctx)
{
    CARD32 y_stride = ctx->yStride;
    CARD32 uv_stride = y_stride >> 1;
    
    BEGIN_HEADER6_DATA(xl, 1);
    OUT_RING_QW_AGP(xl, 0xc50, (y_stride >> 3) | ((uv_stride >> 3) << 16)); 
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);
}


void 
viaVideoSetSWFLipLocked(XvMCLowLevel *xl,
			unsigned yOffs,
			unsigned uOffs,
			unsigned vOffs) {
    int proReg=0;
    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;    

#ifdef VIDEO_DMA
    BEGIN_HEADER6_DATA(xl, 3);
    if (xl->chipId == PCI_CHIP_VT3259)
    {
        OUT_RING_QW_AGP(xl, (proReg|HQV_SRC_OFFSET) + 0x200 , 0);
    }

    OUT_RING_QW_AGP(xl, (proReg|HQV_SRC_STARTADDR_Y) + 0x200, yOffs);
    
    if (xl->chipId == PCI_CHIP_VT3259)
    {
        OUT_RING_QW_AGP(xl, (proReg|HQV_SRC_STARTADDR_U) + 0x200, vOffs);
    }
    else
    {
        OUT_RING_QW_AGP(xl, HQV_SRC_STARTADDR_U + 0x200, uOffs);
        OUT_RING_QW_AGP(xl, HQV_SRC_STARTADDR_V + 0x200 , vOffs);
    }
    WAITFLAGS(xl, LL_MODE_VIDEO); 
#else
    pciCommand(xl, VIA_AGP_HEADER6, 3, LL_MODE_VIDEO);
    pciCommand(xl, 0x00F60000, 0, 0);
    if (xl->chipId == PCI_CHIP_VT3259)
        pciCommand(xl, proReg|HQV_SRC_OFFSET + 0x200 , 0, 0);
    pciCommand(xl, proReg|HQV_SRC_STARTADDR_Y + 0x200, yOffs, 0);
    if (xl->chipId == PCI_CHIP_VT3259)
    {
        pciCommand(xl, proReg|HQV_SRC_STARTADDR_U + 0x200, vOffs, 0);
    }
    {
        pciCommand(xl, HQV_SRC_STARTADDR_U + 0x200, uOffs, 0);
        pciCommand(xl, HQV_SRC_STARTADDR_V + 0x200 , vOffs, 0);
    }
    else
#endif
}
    
void 
viaVideoSWFlipLocked(XvMCLowLevel *xl, unsigned flags,
		     int progressiveSequence)
{
    int proReg=0;
    CARD32 andWd,orWd;
    andWd = 0;
    orWd = HQV_ENABLE;

    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;    
    
    if ((flags & XVMC_FRAME_PICTURE) == XVMC_BOTTOM_FIELD) {
	andWd = 0xFFFFFFFFU;
	orWd = HQV_FIELD_UV   |
	    HQV_DEINTERLACE   |
	    HQV_FIELD_2_FRAME |
	    HQV_FRAME_2_FIELD |
	    HQV_YUV420        |
	    HQV_SW_FLIP       |
	    HQV_FLIP_STATUS   |
	    HQV_SUBPIC_FLIP;
    } else if ((flags & XVMC_FRAME_PICTURE) == XVMC_TOP_FIELD) {
	andWd = ~HQV_FLIP_ODD;
	orWd = HQV_FIELD_UV   |
	    HQV_DEINTERLACE   |
	    HQV_FIELD_2_FRAME |
	    HQV_FRAME_2_FIELD |
	    HQV_YUV420        |
	    HQV_FLIP_ODD      |
	    HQV_SW_FLIP       |
	    HQV_FLIP_STATUS   |
	    HQV_SUBPIC_FLIP;
    } else if ((flags & XVMC_FRAME_PICTURE) == XVMC_FRAME_PICTURE) {
	andWd = ~(HQV_DEINTERLACE   | 
		  HQV_FRAME_2_FIELD |
		  HQV_FIELD_2_FRAME |
		  HQV_FIELD_UV);
	orWd = HQV_YUV420 | 
	    HQV_SW_FLIP   |
	    HQV_FLIP_STATUS  |
	    HQV_SUBPIC_FLIP;
    }	    
    if (progressiveSequence) {
        andWd &= ~HQV_FIELD_UV;
        orWd &= ~HQV_FIELD_UV;
    }
    
#ifdef VIDEO_DMA
    syncVideo(xl,1);
    BEGIN_HEADER6_DATA(xl, 1);
    OUT_RING_QW_AGP(xl, (proReg|HQV_CONTROL) + 0x200 , HQV_ENABLE | HQV_GEN_IRQ | orWd);
    WAITFLAGS(xl, LL_MODE_VIDEO);
    agpFlush(xl);
#else
    pciCommand(xl, VIA_AGP_HEADER6, 1, LL_MODE_VIDEO);
    pciCommand(xl, 0x00F60000, 0, 0);
    pciCommand(xl, (proReg|HQV_CONTROL) + 0x200 , HQV_ENABLE | HQV_GEN_IRQ | orWd , 0);
    pciCommand(xl, 0, 0, 0);
#endif
}

	
void 
viaMpegSetFB(XvMCLowLevel *xl,unsigned i,
	     unsigned yOffs,
	     unsigned uOffs,
	     unsigned vOffs) {

    if (xl->chipId == PCI_CHIP_VT3259)
    {
        i *= (4*2);
        BEGIN_HEADER6_DATA(xl, 2);
        OUT_RING_QW_AGP(xl, 0xc28 + i, yOffs >> 3);
        OUT_RING_QW_AGP(xl, 0xc2c + i, vOffs >> 3);
    }
    else
    {
        i *= (4*3);
        BEGIN_HEADER6_DATA(xl, 3);
        OUT_RING_QW_AGP(xl, 0xc20 + i, yOffs >> 3);
        OUT_RING_QW_AGP(xl, 0xc24 + i, uOffs >> 3);
        OUT_RING_QW_AGP(xl, 0xc28 + i, vOffs >> 3);
    }

    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);
}

void 
viaMpegBeginPicture(XvMCLowLevel *xl,ViaXvMCContext *ctx,
		    unsigned width,
		    unsigned height,
		    const XvMCMpegControl *control) {
				  
    unsigned j, mb_width, mb_height;
    mb_width = (width + 15) >> 4;

    mb_height =
	((control->mpeg_coding == XVMC_MPEG_2) && 
	 (control->flags & XVMC_PROGRESSIVE_SEQUENCE)) ?
	2*((height+31) >> 5) : (((height+15) >> 4));

    BEGIN_HEADER6_DATA(xl, 72);
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);

    OUT_RING_QW_AGP(xl, 0xc00,
		    ((control->picture_structure & XVMC_FRAME_PICTURE) << 2) |
		    ((control->picture_coding_type & 3) << 4) |
		    ((control->flags & XVMC_ALTERNATE_SCAN) ? (1 << 6) : 0));
   
    if (!(ctx->intraLoaded)) {
	OUT_RING_QW_AGP(xl, 0xc5c, 0);
	for (j = 0; j < 64; j += 4) {
            OUT_RING_QW_AGP(xl, 0xc60,  
			    ctx->intra_quantiser_matrix[j] |
			    (ctx->intra_quantiser_matrix[j+1] << 8) |
			    (ctx->intra_quantiser_matrix[j+2] << 16) |
			    (ctx->intra_quantiser_matrix[j+3] << 24));
        }
	ctx->intraLoaded = 1;
    }

    if (!(ctx->nonIntraLoaded)) {
	OUT_RING_QW_AGP(xl, 0xc5c, 1);
	for (j = 0; j < 64; j += 4) {
            OUT_RING_QW_AGP(xl, 0xc60,  
			    ctx->non_intra_quantiser_matrix[j] |
			    (ctx->non_intra_quantiser_matrix[j+1] << 8) |
			    (ctx->non_intra_quantiser_matrix[j+2] << 16) |
			    (ctx->non_intra_quantiser_matrix[j+3] << 24));
        }
	ctx->nonIntraLoaded = 1;
    }

    if (!(ctx->chromaIntraLoaded)) {
	OUT_RING_QW_AGP(xl, 0xc5c, 2);
	for (j = 0; j < 64; j += 4) {
            OUT_RING_QW_AGP(xl, 0xc60,  
			    ctx->chroma_intra_quantiser_matrix[j] |
			    (ctx->chroma_intra_quantiser_matrix[j+1] << 8) |
			    (ctx->chroma_intra_quantiser_matrix[j+2] << 16) |
			    (ctx->chroma_intra_quantiser_matrix[j+3] << 24));
        }
	ctx->chromaIntraLoaded = 1;
    }

    if (!(ctx->chromaNonIntraLoaded)) {
	OUT_RING_QW_AGP(xl, 0xc5c, 3);
	for (j = 0; j < 64; j += 4) {
            OUT_RING_QW_AGP(xl, 0xc60,  
			    ctx->chroma_non_intra_quantiser_matrix[j] |
			    (ctx->chroma_non_intra_quantiser_matrix[j+1] << 8) |
			    (ctx->chroma_non_intra_quantiser_matrix[j+2] << 16) |
			    (ctx->chroma_non_intra_quantiser_matrix[j+3] << 24));
        }
	ctx->chromaNonIntraLoaded = 1;
    }
    
    OUT_RING_QW_AGP(xl, 0xc90,  
		    ((mb_width * mb_height) & 0x3fff) |
		    ((control->flags & XVMC_PRED_DCT_FRAME) ? ( 1 << 14)  : 0) |
		    ((control->flags & XVMC_TOP_FIELD_FIRST) ? (1 << 15) : 0 )  |
		    ((control->mpeg_coding == XVMC_MPEG_2) ? (1 << 16) : 0) |
		    ((mb_width & 0xff) << 18));
    	
    OUT_RING_QW_AGP(xl, 0xc94,
		    ((control->flags & XVMC_CONCEALMENT_MOTION_VECTORS) ? 1 : 0) |
		    ((control->flags & XVMC_Q_SCALE_TYPE) ? 2 : 0) |
		    ((control->intra_dc_precision & 3) << 2) |
		    (((1 + 0x100000 / mb_width) & 0xfffff) << 4) |
		    ((control->flags & XVMC_INTRA_VLC_FORMAT) ? (1 << 24) : 0));

    OUT_RING_QW_AGP(xl, 0xc98, 
		    (((control->FHMV_range) & 0xf) << 0) |
		    (((control->FVMV_range) & 0xf) << 4) |
		    (((control->BHMV_range) & 0xf) << 8) |
		    (((control->BVMV_range) & 0xf) << 12) |
		    ((control->flags & XVMC_SECOND_FIELD) ?  (1 << 20) : 0) | 
		    (0x0a6 << 16));

}



void 
viaMpegReset(XvMCLowLevel *xl)
{
    int i,j;

    BEGIN_HEADER6_DATA(xl, 50);
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);

    OUT_RING_QW_AGP(xl, 0xcf0 ,0);

    for (i = 0; i < 6; i++) {
        OUT_RING_QW_AGP(xl, 0xcc0 ,0);
        OUT_RING_QW_AGP(xl, 0xc0c, 0x43|0x20 );
        for (j = 0xc10; j < 0xc20; j += 4)
	        OUT_RING_QW_AGP(xl, j, 0);
    }

    OUT_RING_QW_AGP(xl, 0xc0c, 0x1c3);
    for (j = 0xc10; j < 0xc20; j += 4)
	    OUT_RING_QW_AGP(xl,j,0);

    for (i = 0; i < 19; i++)
        OUT_RING_QW_AGP(xl, 0xc08 ,0);
    
    OUT_RING_QW_AGP(xl, 0xc98, 0x400000);

    for (i = 0; i < 6; i++) {
    OUT_RING_QW_AGP(xl, 0xcc0 ,0);
    OUT_RING_QW_AGP(xl, 0xc0c, 0x1c3|0x20);
    for (j = 0xc10; j < 0xc20; j += 4)
	    OUT_RING_QW_AGP(xl,j,0);
    }
    OUT_RING_QW_AGP(xl, 0xcf0 ,0);

}

void 
viaMpegWriteSlice(XvMCLowLevel *xl, CARD8* slice, int nBytes, CARD32 sCode)
{
    int i, n, r;
    CARD32* buf;
    int count;

    if (xl->errors & (LL_DECODER_TIMEDOUT |
		      LL_IDCT_FIFO_ERROR  |
		      LL_SLICE_FIFO_ERROR |
		      LL_SLICE_FAULT)) return;

    n = nBytes >> 2;
    if (sCode) nBytes += 4;
    r = nBytes & 3;
    buf = (CARD32*) slice;

    if (r) nBytes += 4 - r;

    nBytes += 8;

    BEGIN_HEADER6_DATA(xl, 2);
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);
    OUT_RING_QW_AGP(xl, 0xc9c, nBytes);
    
    if (sCode) OUT_RING_QW_AGP(xl, 0xca0, sCode);

    i = 0;
    count = 0;

    do {
	count += (LL_AGP_CMDBUF_SIZE -20);
	count = (count > n) ? n : count;
	BEGIN_HEADER5_DATA(xl, (count - i), 0xca0);

	for (; i < count; i++) {
	    OUT_RING_AGP(xl, *buf++);
	}
	finish_header_agp(xl);
    } while (i < n);

    BEGIN_HEADER5_DATA(xl, 3, 0xca0);

    if (r) {
	OUT_RING_AGP(xl, *buf & ((1 << (r << 3)) - 1));
    }
    OUT_RING_AGP(xl,0);
    OUT_RING_AGP(xl,0);
    finish_header_agp(xl);
}

void 
viaVideoSubPictureOffLocked(XvMCLowLevel *xl) {

    CARD32 stride;
    int proReg=0;
    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;        

    stride = VIDIN(xl,proReg|SUBP_CONTROL_STRIDE);
#ifdef VIDEO_DMA
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);
    BEGIN_HEADER6_DATA(xl,1);
    OUT_RING_QW_AGP(xl, proReg|SUBP_CONTROL_STRIDE | 0x200, stride & ~SUBP_HQV_ENABLE);
#else
    pciCommand(xl, VIA_AGP_HEADER6, 1, LL_MODE_VIDEO);
    pciCommand(xl, 0x00F60000, 0, 0);
    pciCommand(xl, proReg|SUBP_CONTROL_STRIDE | 0x200, stride & ~SUBP_HQV_ENABLE, 0);
    pciCommand(xl, 0, 0, 0);
#endif

}

void 
viaVideoSubPictureLocked(XvMCLowLevel *xl,ViaXvMCSubPicture *pViaSubPic) {

    unsigned i;
    CARD32 cWord;
    
    int proReg=0;
    if (xl->chipId == PCI_CHIP_VT3259) proReg = REG_HQV1_INDEX;    

#ifdef VIDEO_DMA
    WAITFLAGS(xl, LL_MODE_DECODER_IDLE);
    BEGIN_HEADER6_DATA(xl, VIA_SUBPIC_PALETTE_SIZE + 2);
#else
    pciCommand(xl, VIA_AGP_HEADER6, VIA_SUBPIC_PALETTE_SIZE + 2, LL_MODE_VIDEO);
    pciCommand(xl, 0x00F60000, 0, 0);
#endif
    for (i=0; i<VIA_SUBPIC_PALETTE_SIZE; ++i) {
#ifdef VIDEO_DMA
	OUT_RING_QW_AGP(xl, proReg|RAM_TABLE_CONTROL | 0x200, pViaSubPic->palette[i]);
#else
	pciCommand(xl, proReg|RAM_TABLE_CONTROL | 0x200, pViaSubPic->palette[i], 0);
#endif
    }

    cWord = (pViaSubPic->stride & SUBP_STRIDE_MASK) | SUBP_HQV_ENABLE;
    cWord |= (pViaSubPic->ia44) ? SUBP_IA44 : SUBP_AI44;
#ifdef VIDEO_DMA
    OUT_RING_QW_AGP(xl, proReg|SUBP_STARTADDR | 0x200, pViaSubPic->offset);
    OUT_RING_QW_AGP(xl, proReg|SUBP_CONTROL_STRIDE | 0x200, cWord);
#else
    pciCommand(xl, proReg|SUBP_STARTADDR | 0x200, pViaSubPic->offset, 0);
    pciCommand(xl, proReg|SUBP_CONTROL_STRIDE | 0x200, cWord, 0);
#endif
}

void 
viaBlit(XvMCLowLevel *xl,unsigned bpp,unsigned srcBase,
	unsigned srcPitch,unsigned dstBase,unsigned dstPitch,
	unsigned w,unsigned h,int xdir,int ydir, unsigned blitMode, 
	unsigned color) 
{

    CARD32 dwGEMode = 0, srcY=0, srcX, dstY=0, dstX;
    CARD32 cmd;


    if (!w || !h)
        return;

    finish_header_agp(xl);

    switch (bpp) {
    case 16:
        dwGEMode |= VIA_GEM_16bpp;
        break;
    case 32:
        dwGEMode |= VIA_GEM_32bpp;
	break;
    default:
        dwGEMode |= VIA_GEM_8bpp;
        break;
    }

    srcX = srcBase & 31;
    dstX = dstBase & 31;
    switch (bpp) {
    case 16:
        dwGEMode |= VIA_GEM_16bpp;
	srcX >>= 2;
	dstX >>= 2;
        break;
    case 32:
        dwGEMode |= VIA_GEM_32bpp;
	srcX >>= 4;
	dstX >>= 4;
	break;
    default:
        dwGEMode |= VIA_GEM_8bpp;
        break;
    }

    BEGIN_RING_AGP(xl, 20);
    WAITFLAGS(xl, LL_MODE_2D);


    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_GEMODE), dwGEMode);
    cmd = 0; 

    if (xdir < 0) {
        cmd |= VIA_GEC_DECX;
        srcX += (w - 1);
        dstX += (w - 1);
    }
    if (ydir < 0) {
        cmd |= VIA_GEC_DECY;
        srcY += (h - 1);
        dstY += (h - 1);
    }

    switch(blitMode) {
    case VIABLIT_TRANSCOPY:
	OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_SRCCOLORKEY), color);
	OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_KEYCONTROL), 0x4000);
	cmd |= VIA_GEC_BLT | (VIA_BLIT_COPY << 24);
	break;
    case VIABLIT_FILL:
	OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_FGCOLOR), color);
	cmd |= VIA_GEC_BLT | VIA_GEC_FIXCOLOR_PAT | (VIA_BLIT_FILL << 24);
	break;
    default:
	OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_KEYCONTROL), 0x0);
	cmd |= VIA_GEC_BLT | (VIA_BLIT_COPY << 24);
    }	

    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_SRCBASE), (srcBase & ~31) >> 3);
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_DSTBASE), (dstBase & ~31) >> 3);
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_PITCH), VIA_PITCH_ENABLE |
		    (srcPitch >> 3) | (((dstPitch) >> 3) << 16));
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_SRCPOS), ((srcY << 16) | srcX));
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_DSTPOS), ((dstY << 16) | dstX));
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_DIMENSION), (((h - 1) << 16) | (w - 1)));
    OUT_RING_QW_AGP(xl, H1_ADDR(VIA_REG_GECMD), cmd);
}

unsigned 
syncXvMCLowLevel(XvMCLowLevel *xl, unsigned int mode, unsigned int doSleep,
		 CARD32 timeStamp)
{
    unsigned
	errors;

    if (mode == 0) {
	errors = xl->errors;
	xl->errors = 0;
	return errors;
    }

    if ((mode & (LL_MODE_VIDEO | LL_MODE_3D)) || !xl->use_agp) {
	if (xl->performLocking) 
	    hwlLock(xl,0);
	if ((mode != LL_MODE_VIDEO))
	    syncDMA(xl, doSleep);
	if (mode & LL_MODE_3D) 
	    syncAccel(xl, mode, doSleep);
	if (mode & LL_MODE_VIDEO)
	    syncVideo(xl, doSleep);
	if (xl->performLocking) 
	    hwlUnlock(xl,0);
    } else {
	viaDMAWaitTimeStamp(xl, timeStamp, doSleep);
    }

    if (mode & (LL_MODE_DECODER_SLICE | LL_MODE_DECODER_IDLE)) 
	syncMpeg(xl, mode, doSleep);

    errors = xl->errors;
    xl->errors = 0;

    return errors;
}

int 
initXvMCLowLevel(XvMCLowLevel *xl, int fd, drm_context_t *ctx,
		 drmLockPtr hwLock, drmAddress mmioAddress, 
		 drmAddress fbAddress, int useAgp, unsigned chipId ) 
{
    xl->agp_pos = 0;
    xl->pci_pos = 0;
    xl->use_agp = useAgp;
    xl->fd = fd;
    xl->drmcontext = ctx;
    xl->hwLock = hwLock;
    xl->mmioAddress = mmioAddress;
    xl->fbAddress = fbAddress;
    xl->curWaitFlags = 0;
    xl->performLocking = 1;
    xl->errors = 0;
    xl->agpSync = 0;
    xl->agp_mode = 0;
    xl->chipId = chipId;
    return viaDMAInitTimeStamp(xl); 

}

void 
setLowLevelLocking(XvMCLowLevel *xl, int performLocking)
{
    xl->performLocking = performLocking;
}

void 
closeXvMCLowLevel(XvMCLowLevel *xl) 
{
    viaDMACleanupTimeStamp(xl); 
}

