#ifndef MSPINJPEGDEC_H
#define MSPINJPEGDEC_H

#include <stdbool.h> 		//introduced in C99 with true equal 1, and false equal 0
#include "spin1_api.h"
#include "../SpinJPEG.h"

/********************** CONST/MACRO definitions ***********************/


/*----------- Pre-defined user events -----------*/
#define UE_START_DECODER    1

/*----------- Debugging functionality -----------*/
#define DEBUG_MODE	0

#define FOR_PAPER_ICCES


/********************* for timing measurement **********************/
// Enable timer - free running, 32-bit
#define ENABLE_TIMER() tc[T2_CONTROL] = 0x82

// To measure, set timer to 0
#define START_TIMER() tc[T2_LOAD] = 0

// Read timer and compute time (microseconds)
#define READ_TIMER() ((0 - tc[T2_COUNT]) / sark.cpu_clk)



/********************** VARIABLES definitions ***********************/

/*--- global variables here ---*/

cd_t   comp[3]; /* descriptors for 3 components */

PBlock *MCU_buff[10];  /* decoded component buffer */
                  /* between IDCT and color convert */
int    MCU_valid[10];  /* components of above MCU blocks */

PBlock *QTable[4];     /* three quantization tables */
int    QTvalid[4];     /* at most, but seen as four ... */

/*--- picture attributes ---*/
 int x_size, y_size;	/* Video frame size     */
int rx_size, ry_size;	/* down-rounded Video frame size */
                /* in pixel units, multiple of MCU */
int MCU_sx, MCU_sy;  	/* MCU size in pixels   */
int mx_size, my_size;	/* picture size in units of MCUs */
int n_comp;		/* number of components 1,3 */

/*--- processing cursor variables ---*/
int	in_frame, curcomp;   /* frame started ? current component ? */
int	MCU_row, MCU_column; /* current position in MCU unit */

/*--- RGB buffer storage ---*/
uchar *ColorBuffer;   /* MCU after color conversion */
uchar *FrameBuffer;   /* complete final RGB image */
PBlock        *PBuff;           /* scratch pixel buffer */
FBlock        *FBuff;           /* scratch frequency buffer */

/*--- process statistics ---*/
int stuffers;	/* number of stuff bytes in file */
int passed;	/* number of bytes skipped looking for markers */

/*--- Generic, SpiNN-related variables ---*/
uint coreID;
uint tmeas;

/*============= SpiNNaker related ==============*/
/*--- JPG file storage ---*/
//bool sdramImgBufInitialized; --> moved into sdramImgBuf->isOpened
FILE_t *sdramImgBuf;
//uchar *sdramImgBufPtr;
//uint sdramImgBufSize;			// current size of buffer to hold undecoded JPG image in sdram
volatile bool decIsStarted;		// indicate if decoder has started, initialized in resizeImgBuf()
volatile bool closeImgBufAfterDecoding;
volatile bool success;	// is decoding success or aborted?

/*--- Debugging variables ---*/
uint nReceivedChunk;
uint szImgFile;
ushort wImg;	// this info needs to be sent from host when the image data is raw
ushort hImg;	// this will be passed on to the encoder
static uint dmaAllocErrCntr = 0;






/********************** FUNCTION prototypes ***********************/
/*--- Main/helper functions ----*/
void dumpJPG();			// for debugging
void decode(uint arg0, uint arg1);          // decoder main loop
void app_init ();
void resizeImgBuf(uint szFile, uint portSrc); // portSrc can be used to distinguish, if it is for JPEG data or Raw data
void closeImgBuf();			// the opposite of resizeImgBuf
//void aborted_stream(cond_t condition);
void aborted_stream();
void free_structures();
void emitDecodeDone();
void streamResultToPC();	// optional
void sendImgInfoAck();          // send ack to hostPC after receiving Image information
void reportTmeas(uint t);

/*--- Event Handlers ---*/
void hSDP (uint mBox, uint port);
sdp_msg_t sdpResult;
void hDMA (uint tid, uint tag);
void hUEvent(uint eventID, uint arg);


/*-----------------------------------------*/
/* prototypes from parse.c		   */
/*-----------------------------------------*/
uint get_next_MK(FILE_t *fi);
uint get_size(FILE_t *fi);
int init_MCU(void);
int process_MCU(FILE_t *fi);
int	load_quant_tables(FILE_t *fi);
void skip_segment(FILE_t *fi);
void clear_bits();
uchar get_one_bit(FILE_t *fi);
unsigned long get_bits(FILE_t *fi, int number);


/*-----------------------------------------*/
/* prototypes from utils.c		   */
/*-----------------------------------------*/

/* imitating std-C fgetc() */
int fgetc(FILE_t *fObject);
int fseek(FILE_t *fObject, int offset, int whence);
/*-------------------------*/
int ceil_div(int N, int D);
int floor_div(int N, int D);
void reset_prediction();
int reformat(unsigned long S, int good);
void color_conversion(void);

/*-------------------------------------------*/
/* prototypes from table_vld.c or tree_vld.c */
/*-------------------------------------------*/

int	load_huff_tables(FILE_t *fi);
uchar get_symbol(FILE_t *fi, int select);

/*-----------------------------------------*/
/* prototypes from huffman.c 		   */
/*-----------------------------------------*/
/* unpack, predict, dequantize, reorder on store */
void unpack_block(FILE_t *fi, FBlock *T, int comp);

/*-------------------------------------------*/
/* prototypes from idct.c                    */
/*-------------------------------------------*/

void	IDCT(const FBlock *S, PBlock *T);


#endif
