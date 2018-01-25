/* This is the common header file used by both the worker and the master.
   Inspired by jpeg.h header for all jpeg code from Pierre Guerrier, march 1998     */

#ifndef SPINJPEG_H
#define SPINJPEG_H

/*----------------------------------*/
/* JPEG format parsing markers here */
/*----------------------------------*/

#define SOI_MK	0xFFD8		/* start of image	*/
#define APP_MK	0xFFE0		/* custom, up to FFEF */
#define COM_MK	0xFFFE		/* commment segment	*/
#define SOF_MK	0xFFC0		/* start of frame	*/
#define SOS_MK	0xFFDA		/* start of scan	*/
#define DHT_MK	0xFFC4		/* Huffman table	*/
#define DQT_MK	0xFFDB		/* Quant. table		*/
#define DRI_MK	0xFFDD		/* restart interval	*/
#define EOI_MK	0xFFD9		/* end of image		*/
#define MK_MSK	0xFFF0

#define RST_MK(x)	( (0xFFF8&(x)) == 0xFFD0 )
                        /* is x a restart interval ? */

/*-------------------------------------------------------- */
/* all kinds of macros here				*/
/*-------------------------------------------------------- */

#define first_quad(c)   ((c) >> 4)        /* first 4 bits in file order */
#define second_quad(c)  ((c) & 15)

#define HUFF_ID(hclass, id)       (2 * (hclass) + (id))

#define DC_CLASS        0
#define AC_CLASS        1

/*-------------------------------------------------------*/
/* JPEG data types here					*/
/*-------------------------------------------------------*/

typedef union {		/* block of pixel-space values */
  unsigned char	block[8][8];
  unsigned char	linear[64];
} PBlock;

typedef union {		/* block of frequency-space values */
  int block[8][8];
  int linear[64];
} FBlock;


/* component descriptor structure */

typedef struct {
  unsigned char	CID;	/* component ID */
  unsigned char	IDX;	/* index of first block in MCU */

  unsigned char	HS;	/* sampling factors */
  unsigned char	VS;
  unsigned char	HDIV;	/* sample width ratios */
  unsigned char	VDIV;

  char		QT;	/* QTable index, 2bits 	*/
  char		DC_HT;	/* DC table index, 1bit */
  char		AC_HT;	/* AC table index, 1bit */
  int		PRED;	/* DC predictor value */
} cd_t;


// imitate FILE descriptor
#define EOF -1

typedef enum whence_e {
	SEEK_SET,	// Beginning of file
	SEEK_CUR,	// Current position of the file pointer
	SEEK_END	// End of file
} whence_t;

typedef struct file_descriptor {
	unsigned char *stream;		// normally should be in sdram
	unsigned char *ptrWrite;	// point to a location to put an incoming stream
	unsigned char *ptrRead;		// will be used in fgetc()
	unsigned char isOpened;		// replacing sdramImgBufInitialized (0=closed/FALSE, 1=opened/TRUE)
	unsigned int szFile;		// the original file size
	unsigned int szBuffer;		// the prepared buffer (using rounding up mechanism)
	unsigned int nCharRead;
}FILE_t;


/******************** MULTICAST MECHANISM **********************/
#define FRPL_NEW_RAW_IMG_ADDR_KEY       0x00000002
#define FRPL_NEW_RAW_IMG_INFO_KEY       0x00000001
#define FRPL_MASK                       0xFFFFFFFF


/*********************** SDP MECHANISM *************************/
/*--- For JPEG file ---*/
#define SDP_PORT_JPEG_DATA	1
#define SDP_PORT_JPEG_INFO	2
/*--- For Raw RGB file ---*/
#define SDP_PORT_RAW_DATA   3
#define SDP_PORT_RAW_INFO    4
/*--- For both ---*/
#define SDP_CMD_INIT_SIZE	1
#define SDP_CMD_CLOSE_IMAGE	2
#define SDP_CMD_REPORT_TMEAS    3

#define SDP_RECV_CORE_ID	2	// this will be the master of mSpinJPEGdec
#define DEC_MASTER_CORE		SDP_RECV_CORE_ID
#define ENC_MASTER_CORE		10

#define SDP_SEND_RESULT_TAG     1
#define SDP_SEND_RESULT_PORT    30000
#ifndef SDP_HOST_IP             // can be specified during compile time
#define SDP_HOST_IP		0x02F0A8C0	// This correspond to 192.168.240.2
#endif
#define SDP_TX_TIMEOUT          750             // reduced throughput but save for VGA image
//#define SDP_TX_TIMEOUT          500             // can go save at almost 5Mbps
//#define SDP_TX_TIMEOUT          10000000      // for debugging


/*----------- DMA functionality -----------*/
#define DMA_JPG_IMG_BUF_WRITE	1
#define DMA_RAW_IMG_BUF_WRITE   2


#endif
