/* Version log
 * 0.1 Initial version
 * 0.2 GUI improvement
 * 0.3 Including spin5
 * 0.4 Using MCPL for forwarding pixel values
 * 0.5 Use reply for sending result to host
 *
 * TODO:
 * - UDP communication is still troublesome:
 *   1. IndexError: bytearray index out of range
 *   2. too slow (due to delay)
 *   3. How to solve SDP_TX_TIMEOUT?
 *
 * Catatanku:
 * - Masalah dengan teknik handshaking:
 *   Misalkan saat yang kirim adalah chip<0,0>, maka tidak bisa bisa kirim reply ke chip<0,0>
 *   karena dia sibuk. Terpaksa kirim ke chip yang lain (misal chip<1,1>), dan chip<1,1> tersebut
 *   akan kirim MCPL yang menandai ada kiriman reply. Masalahnya, ketika tiba waktunya
 *   chip<1,1>, maka handshaking harus dihandle yang lain (misal chip<0,0>), dan ini agak
 *   merepotkan programmingnya.
 * - Aku coba hitung berapa paket yang dikirim saat SDP_TX_TIMEOUT==0
 *   dan ternyata memang ada beberapa paket yang di-drop. Dalam kondisi normal,
 *   paket yang terkirim akan selang-seling 282-86, tapi saat SDP_TX_TIMEOUT==0.
 *   paket yang terkirim jadi kacau (tidak selalu urut 282-86).
 * */
#ifndef SPINNEDGE_H
#define SPINNEDGE_H

#define PROG_VERSION	0.5
#define MAJOR_VERSION	0
#define MINOR_VERSION	5


/*--------------------------------- timer 2 ----------------------------------*/
// Use "timer2" to measure elapsed time.
// Times up to around 10 sec should be OK.

#define CLK_FREQ	200		// karena mungkin kita akan coba lain freq

// Enable timer - free running, 32-bit
#define ENABLE_TIMER() tc[T2_CONTROL] = 0x82

// To measure, set timer to 0
#define START_TIMER() tc[T2_LOAD] = 0

// Read timer and compute time (microseconds)
//#define READ_TIMER() ((0 - tc[T2_COUNT]) / sark.cpu_clk)
#define READ_TIMER() ((0 - tc[T2_COUNT]) / CLK_FREQ)

/*-----------------------------------------------------------------------------*/

//#define USE_SPIN3
#define USE_SPIN5

#define USE_MCPL_FOR_FWD

#include <spin1_api.h>

/* 3x3 GX and GY Sobel mask.  Ref: www.cee.hw.ac.uk/hipr/html/sobel.html */
static const short GX[3][3] = {{-1,0,1},
				 {-2,0,2},
				 {-1,0,1}};
static const short GY[3][3] = {{1,2,1},
				 {0,0,0},
				 {-1,-2,-1}};

/* Laplace operator: 5x5 Laplace mask.  Ref: Myler Handbook p. 135 */
static const short LAP[5][5] = {{-1,-1,-1,-1,-1},
				  {-1,-1,-1,-1,-1},
				  {-1,-1,24,-1,-1},
				  {-1,-1,-1,-1,-1},
				  {-1,-1,-1,-1,-1}};

/* Gaussian filter. Ref:  en.wikipedia.org/wiki/Canny_edge_detector */
static const short FILT[5][5] = {{2,4,5,4,2},
				   {4,9,12,9,4},
				   {5,12,15,12,5},
				   {4,9,12,9,4},
				   {2,4,5,4,2}};
static const short FILT_DENOM = 159;

#define SDP_TX_TIMEOUT          500
//#define SDP_TX_TIMEOUT          10000000

#define MAX_NODES				48	// for Spin5, at the moment

#define TIMER_TICK_PERIOD_US 	1000000
#define PRIORITY_LOWEST         4
#define PRIORITY_TIMER			3
#define PRIORITY_PROCESSING		2
#define PRIORITY_SDP			1
#define PRIORITY_MCPL			-1
#define PRIORITY_DMA			0

#define SDP_TAG_REPLY			1
#define SDP_UDP_REPLY_PORT		20000
#define SDP_HOST_IP				0x02F0A8C0	// 192.168.240.2, dibalik!
#define SDP_TAG_RESULT			2
#define SDP_UDP_RESULT_PORT		20001
#define SDP_TAG_DEBUG           3
#define SDP_UDP_DEBUG_PORT      20002

#define SDP_PORT_R_IMG_DATA		1
#define SDP_PORT_G_IMG_DATA		2
#define SDP_PORT_B_IMG_DATA		3
#define SDP_PORT_ACK			6
#define SDP_PORT_CONFIG			7
#define SDP_CMD_CONFIG			1	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_CONFIG_CHAIN	11
#define SDP_CMD_PROCESS			2	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_CLEAR			3	// will be sent via SDP_PORT_CONFIG
#define SDP_CMD_ACK_RESULT		4

#define SDP_CMD_HOST_SEND_IMG	0x1234
#define SDP_CMD_REPLY_HOST_SEND_IMG	0x4321
#define IMG_OP_SOBEL_NO_FILT	1	// will be carried in arg2.low
#define IMG_OP_SOBEL_WITH_FILT	2	// will be carried in arg2.low
#define IMG_OP_LAP_NO_FILT		3	// will be carried in arg2.low
#define IMG_OP_LAP_WITH_FILT	4	// will be carried in arg2.low
#define IMG_NO_FILTERING		0
#define IMG_WITH_FILTERING		1
#define IMG_SOBEL				0
#define IMG_LAPLACE				1

#define IMG_R_BUFF0_BASE		0x61000000
#define IMG_G_BUFF0_BASE		0x62000000
#define IMG_B_BUFF0_BASE		0x63000000
#define IMG_R_BUFF1_BASE		0x64000000
#define IMG_G_BUFF1_BASE		0x65000000
#define IMG_B_BUFF1_BASE		0x66000000

//we also has direct key to each core (see initRouter())
#define MCPL_BCAST_INFO_KEY		0xbca50001	// for broadcasting ping and blkInfo
#define MCPL_BCAST_CMD_FILT		0xbca50002	// command for filtering only
#define MCPL_BCAST_CMD_DETECT	0xbca50003  // command for edge detection
#define MCPL_BCAST_GET_WLOAD	0xbca50004	// trigger workers to compute workload
#define MCPL_PING_REPLY			0x1ead0001
#define MCPL_FILT_DONE			0x1ead0002	// worker send signal to leader
#define MCPL_EDGE_DONE			0x1ead0003
#define MCPL_BCAST_SEND_RESULT	0xbca50005	// broadcasting host acknowledge
#define MCPL_BCAST_HOST_ACK     0xbca50006

// special key with base value 0xbca5FFF
#define MCPL_BCAST_PIXEL_BASE	0xbca60000
#define MCPL_BCAST_RED_PX       0xbca60001  // the pixels are in the payload
#define MCPL_BCAST_RED_PX_END   0xbca60002  // notify next node that image channel is complete
#define MCPL_BCAST_GREEN_PX     0xbca60003  // FFF part contains information about dLen
#define MCPL_BCAST_GREEN_PX_END 0xbca60004
#define MCPL_BCAST_BLUE_PX      0xbca60005
#define MCPL_BCAST_BLUE_PX_END  0xbca60006
#define MCPL_BCAST_IMG_READY    0xbca6FFF7

// special key for forwarding image configuration
#define MCPL_BCAST_IMG_INFO_BASE	0xbca70000	// start of info chain
#define MCPL_BCAST_IMG_INFO_SIZE	0xbca70001
#define MCPL_BCAST_IMG_INFO_NODE	0xbca70002
#define MCPL_BCAST_IMG_INFO_OPR		0xbca70003
#define MCPL_BCAST_IMG_INFO_EOF		0xbca70004	// end of info

//special key (with values)
#define MCPL_BLOCK_DONE			0x1ead1ead	// should be sent to <0,0,1>

//some definitions
#define FLAG_FILTERING_DONE		0xf117
#define FLAG_DETECTION_DONE		0xde7c

typedef struct block_info {
	ushort wImg;
	ushort hImg;
	ushort isGrey;			// 0==color, 1==gray
	uchar opType;			// 0==sobel, 1==laplace
	uchar opFilter;			// 0==no filtering, 1==with filtering
	ushort nodeBlockID;		// will be send by host
	ushort maxBlock;		// will be send by host
	// then pointers to the image in SDRAM
	uchar *imgRIn;
	uchar *imgGIn;
	uchar *imgBIn;
	uchar *imgROut;
	uchar *imgGOut;
	uchar *imgBOut;
	// miscellaneous info
	uchar imageInfoRetrieved;
	uchar fullRImageRetrieved;
	uchar fullGImageRetrieved;
	uchar fullBImageRetrieved;
} block_info_t;

// worker info
typedef struct w_info {
	uchar wID[17];			// coreID of all workers (max is 17), hold by leadAp
	uchar subBlockID;		// this will be hold individually by each worker
	uchar tAvailable;		// total available workers, should be initialized to 1
	ushort blkStart;
	ushort blkEnd;
	ushort nLinesPerBlock;
	ushort startLine;
	ushort endLine;
	// helper pointers
	ushort wImg;		// just a copy of block_info_t.wImg
	ushort hImg;		// just a copy of block_info_t.hImg
	uchar *imgRIn;		// each worker has its own value of imgRIn --> workload base
	uchar *imgGIn;		// idem
	uchar *imgBIn;		// idem
	uchar *imgROut;		// idem
	uchar *imgGOut;		// idem
	uchar *imgBOut;		// idem
	uchar *blkImgRIn;	// this will be shared among cores in the same chip
	uchar *blkImgGIn;	// idem
	uchar *blkImgBIn;	// idem
	uchar *blkImgROut;	// idem
	uchar *blkImgGOut;	// idem
	uchar *blkImgBOut;	// idem
} w_info_t;

typedef struct chain {
	ushort x;
	ushort y;
	ushort id;

} chain_t;
ushort nodeCntr;	// to count, how many nodes have been received by non-root node
uchar needSendDebug;

/* Due to filtering mechanism, the address of image might be changed.
 * Scenario:
 * 1. Without filtering:
 *		imgRIn will start from IMG_R_BUFF0_BASE and resulting in IMG_R_BUFF1_BASE
 *		imgGIn will start from IMG_G_BUFF0_BASE and resulting in IMG_G_BUFF1_BASE
 *		imgBIn will start from IMG_B_BUFF0_BASE and resulting in IMG_B_BUFF1_BASE
 * 2. Aftering filtering:
 *		imgRIn will start from IMG_R_BUFF1_BASE and resulting in IMG_R_BUFF0_BASE
 *		imgGIn will start from IMG_G_BUFF1_BASE and resulting in IMG_G_BUFF0_BASE
 *		imgBIn will start from IMG_B_BUFF1_BASE and resulting in IMG_B_BUFF0_BASE
 *
*/

// for dma operation
#define DMA_FETCH_IMG_TAG		0x14
#define DMA_STORE_IMG_TAG		0x41
uint szDMA;
#define DMA_MOVE_IMG_R			0x52
#define DMA_MOVE_IMG_G			0x47
#define DMA_MOVE_IMG_B			0x42

uint myCoreID;
w_info_t workers;
block_info_t *blkInfo;			// let's put in sysram, to be shared with workers
uchar nFiltJobDone;				// will be used to count how many workers have
uchar nEdgeJobDone;				// finished their job in either filtering or edge detection
uchar nBlockDone;
//chain_t *chips;					// list of chips in a chain for image loading
chain_t chips[MAX_NODES];
uchar chainMode;				// 0=perchip, 1=use chain
ushort ackCntr;

volatile uint64 tic, toc;
volatile ushort elapse;

uint pixelBuffer[(sizeof(cmd_hdr_t) + 256)/sizeof(uint)];
ushort pixelCntr;

// Pelajaran hari ini: Jangan taruh static di sdp_msg_t, akibatnya
// isi variabel jadi kacau. Mungkin karena ukuran memori statis di sark dibatasi?
sdp_msg_t reportMsg;			// in python, this will be handled in blocking mode (via native socket)
sdp_msg_t resultMsg;			// in python, this will be handled by QtNetwork.QUdpSocket
sdp_msg_t debugMsg;				// in python, this will be handled by QtNetwork.QUdpSocket

// forward declaration
void triggerProcessing(uint arg0, uint arg1);	// after filterning, leadAp needs to copy
													// the content from FIL_IMG to ORG_IMG
void imgDetection(uint arg0, uint arg1);	// this might be the most intense function
void imgFiltering(uint arg0, uint arg1);	// this is separate operation from edge detection
void initSDP();
void initRouter();
void initImage();
void initIPTag();
void hDMADone(uint tid, uint tag);
void hTimer(uint tick, uint Unused);
void hMCPL(uint key, uint payload);
void hSDP(uint mBox, uint port);
void imgFiltering(uint arg0, uint arg1);
void imgProcessing(uint arg0, uint arg1);
void cleanUp();
void computeWLoad(uint withReport, uint arg1);
void afterFiltDone(uint arg0, uint arg1);
void afterEdgeDone(uint arg0, uint arg1);
void sendResult(uint arg0, uint arg1);
void notifyHostDone(uint arg0, uint arg1);	// inform host that all results have been sent
void collectPixel(uint key, uint payload);  // for MCPL-base forwarding
void afterCollectPixel(uint port, uint Unused);
void initNode(uint arg0, uint arg1);

uchar *dtcmImgBuf;
// for fetching/storing image
volatile uchar dmaImgFromSDRAMdone;
// also for copying image from sdp to sdram via dma
volatile uchar dmaImg2SDRAMdone;
volatile ushort dLen;
volatile uchar whichRGB;	//0=R, 1=G, 2=B
volatile uchar hostAck;

// helper/debugging functions
void printImgInfo(uint opType, uint None);
void printWID(uint None, uint Neno);
void myDelay();
void sendReply(uint arg0, uint arg1);
void printWLoad();

// for debugging:
static ushort linesR = 0;
static ushort linesG = 0;
static ushort linesB = 0;

#endif // SPINNEDGE_H

