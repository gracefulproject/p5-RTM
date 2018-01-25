/* Based on jpec by Moodstocks SAS (2012-2016)
 *
 * */
#ifndef MSPINJPEGENC_H
#define MSPINJPEGENC_H

#include <spin1_api.h>
#include <stdbool.h>
#include "conf.h"
#include "../SpinJPEG.h"

/********************** MACROS/DATATYPES definitions ***********************/

#define FOR_PAPER_ICCES

#define DEBUG_MODE	0

#define DMA_FETCH_IMG_FOR_SDP_TAG	0x1


#define JPEG_ENC_DEF_QUAL   93 /* default quality factor */
#define JPEC_ENC_HEAD_SIZ  330 /* header typical size in bytes */
#define JPEC_ENC_BLOCK_SIZ  30 /* 8x8 entropy coded block typical size in bytes */
//#define JPEC_BUFFER_INIT_SIZ 65536
#define JPEC_BUFFER_INIT_SIZ 16385	// DTCM max is 64KB

/* A simple code can verify that:
Size of jpec_block_t = 772
Size of jpec_huff_skel_t = 24
Size of jpec_enc_t = 1080
*/

/** Extensible byte buffer */
typedef struct jpec_buffer_t_ {
  uchar *stream;                      /* byte buffer --> must be in sdram */
  int len;                              /* current length */
  int siz;                              /* maximum size */
} jpec_buffer_t;

/** Entropy coding data that hold state along blocks */
typedef struct jpec_huff_state_t_ {
  int buffer;             /* bits buffer */
  int nbits;                  /* number of bits remaining in buffer */
  int dc;                     /* DC coefficient from previous block (or 0) */
  jpec_buffer_t *buf;         /* JPEG global buffer */
} jpec_huff_state_t;

/** Type of an Huffman JPEG encoder */
typedef struct jpec_huff_t_ {
  jpec_huff_state_t state;    /* state from previous block encoding */
} jpec_huff_t;

/** Structure used to hold and process an image 8x8 block */
typedef struct jpec_block_t_ {
  float dct[64];            /* DCT coefficients */
  int quant[64];            /* Quantization coefficients */
  int zz[64];               /* Zig-Zag coefficients */
  int len;                  /* Length of Zig-Zag coefficients */
} jpec_block_t;

/** Skeleton for an Huffman entropy coder */
typedef struct jpec_huff_skel_t_ {
  void *opq;
  void (*del)(void *opq);
  void (*encode_block)(void *opq, jpec_block_t *block, jpec_buffer_t *buf);
} jpec_huff_skel_t;

/** JPEG encoder */
struct jpec_enc_t_ {
  /** Input image data */
  const uchar *img;                   /* image buffer */
  ushort w;                           /* image width */
  ushort h;                           /* image height */
  ushort w8;                          /* w rounded to upper multiple of 8 */
  /** JPEG extensible byte buffer */
  jpec_buffer_t *buf;
  /** Compression parameters */
  int qual;                             /* JPEG quality factor */
  int dqt[64];                          /* scaled quantization matrix */
  /** Current 8x8 block */
  int bmax;                             /* maximum number of blocks (N) */
  int bnum;                             /* block number in 0..N-1 */
  ushort bx;                          /* block start X */
  ushort by;                          /* block start Y */
  jpec_block_t block;                   /* block data */
  /** Huffman entropy coder */
  jpec_huff_skel_t *hskel;
};

/** Type of a JPEG encoder object */
typedef struct jpec_enc_t_ jpec_enc_t;




/********************** VARIABLES definitions ***********************/


/*--- Generic, SpiNN-related variables ---*/
uint coreID;
uchar *sdramImgBuf;
uchar *ptrWrite;
uchar *dtcmImgBuf;
uchar *jpgResult;
int szjpgResult;
ushort wImg;
ushort hImg;
sdp_msg_t sdpResult;
volatile bool dmaImgFromSDRAMdone;
// the following special variable is used during ICCES paper experiment
ushort numProcBlock;


/*--- Encoder related ---*/
static int jpgQ = JPEG_ENC_DEF_QUAL;
jpec_enc_t *e;	// encoder object


/********************** FUNCTION prototypes ***********************/
/*--- SpiNNaker functionalities ---*/
void encode(uint arg0, uint arg1);          // encoder main loop
void app_init ();
#ifndef FOR_PAPER_ICCES
void sendJPG(uint arg0, uint arg1);			// send the result back to host-PC
#endif
void resizeImgBuf(uint szFile, uint portSrc);
void sendResult(uint tmeas);

/*--- Event Handlers ---*/
void hFR (uint key, uint payload);
void hDMA (uint tid, uint tag);
void hSDP (uint mBox, uint port);           // only for testing

/*--- Three Main Encoder functions ---*/
jpec_enc_t *jpec_enc_new(const uchar *img, ushort w, ushort h, int q);  /* Create a JPEG encoder */
uchar *jpec_enc_run(jpec_enc_t *e, int *len);						/* Run the JPEG encoding */
void jpec_enc_del(jpec_enc_t *e);										/* Release a JPEG encoder object */

/*--- Various encoder functions ---*/
// from enc.c
void jpec_enc_init_dqt(jpec_enc_t *e);
void jpec_enc_open(jpec_enc_t *e);
void jpec_enc_close(jpec_enc_t *e);
void jpec_enc_write_soi(jpec_enc_t *e);
void jpec_enc_write_app0(jpec_enc_t *e);
void jpec_enc_write_dqt(jpec_enc_t *e);
void jpec_enc_write_sof0(jpec_enc_t *e);
void jpec_enc_write_dht(jpec_enc_t *e);
void jpec_enc_write_sos(jpec_enc_t *e);
int jpec_enc_next_block(jpec_enc_t *e);
void jpec_enc_block_dct(jpec_enc_t *e);
void jpec_enc_block_quant(jpec_enc_t *e);
void jpec_enc_block_zz(jpec_enc_t *e);

// from buf.c
jpec_buffer_t *jpec_buffer_new(int siz);
void jpec_buffer_del(jpec_buffer_t *b);
void jpec_buffer_write_byte(jpec_buffer_t *b, int val);
void jpec_buffer_write_2bytes(jpec_buffer_t *b, int val);

// from huff.c
/** Skeleton initialization */
void jpec_huff_skel_init(jpec_huff_skel_t *skel);
jpec_huff_t *jpec_huff_new(void);
void jpec_huff_del(jpec_huff_t *h);
void jpec_huff_encode_block(jpec_huff_t *h, jpec_block_t *block, jpec_buffer_t *buf);



#endif

