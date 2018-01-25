#include "gateway.h"
#include <stdfix.h>


// all constants below must be positive
#define R_GRAY                      REAL_CONST(0.2989)
#define G_GRAY                      REAL_CONST(0.5870)
#define B_GRAY                      REAL_CONST(0.1140)

typedef struct rgb_s {
    uchar blue;
    uchar green;
    uchar red;
} rgb_t;

static inline REAL roundr(REAL inVal)
{
    uint base = (uint)inVal;
    uint upper = base + 1;
    REAL conver = inVal + REAL_CONST(0.5);
    if((uint)conver == base)
        return (REAL)base;
    else
        return (REAL)upper;
}

void rgb2gray(uint arg0, uint arg1)
{
    rgb_t *ptrRGB = (rgb_t *)pxBuf.RGBbuf;
    rgb_t pxRGB;
    uint numPx = pxBuf.pxSeqLen >> 16;      // number of pixels in the chunk
    uint segPx = pxBuf.pxSeqLen & 0xFFFF;   // chunk seqment ID

    REAL tmp;
    uchar gVal;

    // prepare the gray buffer
    pxBuf.ptrGraybuf = pxBuf.Graybuf;

    for(uint i=0; i<numPx; i++) {
        sark_mem_cpy((void *)&pxRGB, (void *)ptrRGB, sizeof(rgb_t));

        // compute the gray level
        tmp = (REAL)pxRGB.red * R_GRAY + (REAL)pxRGB.green * G_GRAY + (REAL)pxRGB.blue * B_GRAY;

        // convert the REAL to uchar
        // why round first? because if not, it truncate to lower integer
        gVal = (uchar)roundr(tmp);

        // put to the gray buffer
        *pxBuf.ptrGraybuf++ = gVal;

        // and count for histogram
        histogram[gVal]++;

        ptrRGB++;
    }

    /* then copy to sdram determined by pxBuffer.pxSeqLen
     * Assuming that we only work with "non icon" image, then the chunk size
     * will be the same, unless the last one. Hence, it is safe to put the fix-size here: */
    uint offset = segPx * MAX_PIXEL_IN_CHUNK;
    uint dmaSent, tg;

    do {
        tg = (wInfo.coreID << 16) | DMA_TAG_STORE_Y_PIXELS;
        dmaSent = spin1_dma_transfer(tg, imgBufIn + offset, pxBuf.ptrGraybuf, DMA_WRITE, numPx);
    } while(dmaSent==0);
}

inline void resetHistogram()
{
    histChainCntr = 0;  // this has special meaning for core 2, 6, and 13
    for(uint i=0; i<256; i++)
        histogram[i] = 0;
}


/* When chipFwdr or jpeg_enc send MCPL_FWD_PIXEL_EOF, the gateway performs the following operations:
 * - histogram counting and broadcast it
 * - broadcast gray image stored in the imgBufIn
 *
 * histAndBcast() with phase-0 will be called when LEAD_CORE receives MCPL_FWD_PIXEL_EOF
 * (from chipFwdr or jpeg_enc).
 *
 * Histogram counting will be performed as a chain, started from LEAD_CORE:
 * - LEAD_CORE bradcast FR to core 3,9,10,16 to start phase-1
 * */
void histAndBcast(uint phase, uint arg1)
{
    uint i,key,payload;
    // phase-0, only LEAD_CORE is active
    if(phase==0) {
        // then start the chain with phase-1
        spin1_send_mc_packet(MCPL_HIST_CHAIN_NEXT | wInfo.coreID, 1, WITH_PAYLOAD);
    }
    // for phase-1: core 3,9,10,and 16 forward its histogram
    // for phase-2: core 4,8,11,and 15 forward its histogram
    // for phase-3: core 5,7,12,and 14 forward its histogram
    // for phase-4: core 6 and 13 forward its histogram
    else if(phase<5) {
        i=0;
        key = MCPL_HIST_CHAIN | wInfo.coreID;
        while(i<256){
            payload = (i << 16) | histogram[i];
            spin1_send_mc_packet(key, payload, WITH_PAYLOAD);
            i++;
        }
        key = MCPL_HIST_CHAIN_NEXT | wInfo.coreID;
        spin1_send_mc_packet(key, phase+1, WITH_PAYLOAD); //tell the next core for phase-2

    }
    // for phase-5: bcast histogram and gray image by core 2 to core 12
    else if(phase==5) {
        // LEAD_CORE tells itself and core 3-12 to broadcast histogram and image
        bcastFR(FRPL_BCAST_HIST_GRAY,0,NO_PAYLOAD);
    }
}

/* factorOfFour() is used to allocated buffer with size of factor of four.
 * This is useful because we want to split the buffer and send as MCPL's payload
 * (which is 4-bytes).
 * */
inline uint factorOfFour(uint initialSize)
{
    uint res=initialSize/4;
    uint rem=initialSize%4;
    if(rem!=0) res++;
    return res*4;
}

/* For image broadcasting purpose, 10 cores will be assigned. Hence, they need
 * to know, which region of the image is their part to handle.
 * */
void computeWLoad(uint withReport, uint arg1)
{
    // if the node is not involved, simply disable the core
    if(wInfo.active==FALSE) return;

    // at this point wImg and hImg should be valid already
    ushort i;

    /*--------------------------------------------------------------------------*/
    /*----------------- then compute local region for each core ----------------*/
    ushort nLinesPerCore, nRemInCore;
    ushort wl[BCAST_IMG_NUM_CORE], sp[BCAST_IMG_NUM_CORE], ep[BCAST_IMG_NUM_CORE];

    nLinesPerCore = wInfo.hImg / BCAST_IMG_NUM_CORE;
    nRemInCore = wInfo.hImg % BCAST_IMG_NUM_CORE;

    // initialize number of lines for each core
    for(i=0; i<BCAST_IMG_NUM_CORE; i++)
        wl[i] = nLinesPerCore;

    // then distribute the remaining lines
    i=0; while(nRemInCore-- > 0) wl[i++]++;

    // then compute the blkStart according to wl (block-size)
    sp[0]=0; ep[0]=sp[0]+wl[0]-1;
    for(i=1;i<BCAST_IMG_NUM_CORE;i++) {
        sp[i]=ep[i-1]+1;
        ep[i]=sp[i]+wl[i]-1;
    }

    // then determine, which part belongs to this core
    // at this point, each worker should know where the imgBufIn is
    wInfo.startLine=sp[wInfo.coreID-BCAST_IMG_STARTING_CORE];
    wInfo.endLine=ep[wInfo.coreID-BCAST_IMG_STARTING_CORE];
    wInfo.myImgBufIn=imgBufIn+wInfo.wImg*wInfo.startLine;

    // then allocateDtcmImgBuf() with factorOfFour()
    if(wInfo.dtcmImgBuf!=NULL) sark_free(wInfo.dtcmImgBuf);
    wInfo.sz_dtcmImgBuf=factorOfFour(wInfo.wImg);
    wInfo.dtcmImgBuf=sark_alloc(1,wInfo.sz_dtcmImgBuf);
    if(wInfo.dtcmImgBuf==NULL) {
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[ERR] dtcmImgBuf alloc!\n");
#else
        rt_error(RTE_MALLOC);
#endif
    }
}

/* LEAD_CORE broadcast histogram in chunks
 * the chunk id will be put in the lower word of the key
 * total byte of histogram = 256*2 (because ushort)
 * */
void bcastHistogram(uint arg0, uint arg1)
{
    uint key, payload, i=0;
    uint *ptrHistogram = (uint *)histogram;
    while(i<256){
        key = MCPL_BCAST_HIST | i;
        payload = *ptrHistogram++;
        spin1_send_mc_packet(key, payload, WITH_PAYLOAD);
        i+=2;
    }
}


/* how bcastGrayImg() work?
 * 1. fetch from sdram for sz_dtcmImgBuf byte, wait until dma done
 * 2. split into szMCPLiter chunks, send the chunks, and wait
 *    until vidpro_master send ack
 * 3. repeat step-1 until all part has been sent
 * */
void bcastGrayImg(uint arg0, uint arg1)
{
    if(wInfo.active==FALSE) return;

    uint dmaSent, tg, i;

    // prepare the key and payload for MCPL
    uint *payload;
    uint key;

    // how many MCPL iteration will we have?
    ushort szMCPLiter = wInfo.sz_dtcmImgBuf/4;

    // initially, point to the beginning of my image region
    uchar *ptrMyImgBufIn = wInfo.myImgBufIn;

    for(i=wInfo.startLine; i<=wInfo.endLine; i++) {

        dma_bcastGrayImg_cont = FALSE;
        do {
            tg = (wInfo.coreID << 16) | DMA_TAG_BCAST_IMG;
            // fetch "JUST" a line at a time, exactly at wImg size
            // note: sizeof(dtcmImgBuf) >= sizeof(wImg)
            dmaSent = spin1_dma_transfer(tg, ptrMyImgBufIn,
                                         wInfo.dtcmImgBuf,
                                         DMA_READ, wInfo.wImg);
        } while(dmaSent==0);
        // wait until dma complete
        while(dma_bcastGrayImg_cont==FALSE) {
        }

        // then split into MCPL chunks
        // first tell the line number
        key = MCPL_BCAST_IMG_INFO | wInfo.coreID;
        spin1_send_mc_packet(key, i, WITH_PAYLOAD);
        // then the chunks
        key = MCPL_BCAST_IMG | wInfo.coreID;
        mcpl_bcastGrayImg_cont = FALSE;
        payload=(uint *)ptrMyImgBufIn; // and fetch every 4 bytes
        for(i=0;i<szMCPLiter;i++){
            spin1_send_mc_packet(key, *payload, WITH_PAYLOAD);
        }
        // wait until we receive acknowledge
        while(mcpl_bcastGrayImg_cont==FALSE) {
        }

        // then continue with the next line
        ptrMyImgBufIn += wInfo.wImg; // hence, no "uncharted" tail here
    }


}
