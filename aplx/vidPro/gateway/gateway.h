#ifndef GATEWAY_H
#define GATEWAY_H

#include <spin1_api.h>

#include "../vidPro.h"

#define DEBUG_LEVEL 1

// dma transfer
//#define DMA_TAG_STORE_FRAME		0x10
#define DMA_TAG_STORE_R_PIXELS		0x20
#define DMA_TAG_STORE_G_PIXELS		0x40
#define DMA_TAG_STORE_B_PIXELS		0x80
#define DMA_TAG_STORE_Y_PIXELS      0x100

#define DMA_TAG_BCAST_IMG           0x200

/* Multiple cores receive chunks consecutively. This way, each core
// will have enough time to do grayscaling, histogram, storing/broadcasting.
// NOTE: pxSeq is the chunk seqment and is contained in the seq in
//		 the SDP data sent to root-node.*/
typedef struct pxChunk_s {
    //ushort pxSeq;     // the segment index of the chunk within the overall frame structure
    //uchar pxLen;      // how many pixels in this chunk
    uint  pxSeqLen;     // combination of pxSeq and pxLen
    uint  *RGBbuf;      // temporary buffer to hold fwded RGB pixels
    uint  *ptrRGBbuf;	// pointer to RGBbuf
    uchar *Graybuf;     // for holding grayscale image
    uchar *ptrGraybuf;  // pointer to Graybuf
} pxChunk_t;

/* Initialization */
uint initRouter();
uint initApp();         // includes sanity check
void initHandlers();
void initImgBuf(uint create_or_del, uint keep_buf);

/* Event handler */
void hDMA   (uint tid,  uint tag);
void hSDP   (uint mBox, uint port);
void hMC    (uint key,  uint null);
void hMCPL  (uint key,  uint payload);
void hFR    (uint key,  uint null);
void hFRPL  (uint key,  uint payload);
void bcastFR(uint key, uint payload, uint load_condition);

/* Image Processing */
void rgb2gray(uint arg0, uint arg1);
void resetHistogram();
void histAndBcast(uint phase, uint arg1);
void computeWLoad(uint withReport, uint arg1);
void bcastHistogram(uint arg0, uint arg1);
void bcastGrayImg(uint arg0, uint arg1);

/* Utilities */
uint factorOfFour(uint initialSize);

/* Variables */
frame_info_t frameInfo;
pxChunk_t pxBuf; // will be in DTCM for fast response
w_info_t wInfo;

uint imgBufInitialized;
uchar *imgBufIn;        // in sdram, shared
uchar *imgBufOut;       // in sdram, shared
ushort *histogram;      // in dtcm, each fwding core, during initApp()
uint histChainCntr;     // must be initialized along histogram

volatile uchar dma_bcastGrayImg_cont;    // continue flag for bcastGrayImg()
volatile uchar mcpl_bcastGrayImg_cont;   // continue flag for bcastGrayImg()
#endif
