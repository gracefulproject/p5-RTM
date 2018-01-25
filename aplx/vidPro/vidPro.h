#ifndef VIDPRO_H
#define VIDPRO_H

/* This is common header that will be used by all sub-programs. */

#define PRIORITY_DMA            -1
#define PRIORITY_SDP            0
#define PRIORITY_MCPL           1
#define PRIORITY_FRPL           2
#define PRIORITY_LOW            3       // for a scheduled callback

#define EAST_LINK               0
#define NEAST_LINK              1
#define NORTH_LINK              2
#define WEST_LINK               3
#define SWEST_LINK              4
#define SOUTH_LINK              5

#define PROFILER_CORE           1
#define LEAD_CORE               2       // core-1 is the profiler
#define GATEWAY_CHIP            0x202
#define ROOT_VIDPRO_NODE        0x303

#define GATEWAY_APP_ID          16
#define M_VIDPRO_NODE_APP_ID    18
#define W_VIDPRO_NODE_APP_ID    20
#define JPEG_ENC_APP_ID         100
#define JPEG_DEC_APP_ID         101
#define CHIPFWDR_APP_ID         123
#define PROFILER_RTM_APP_ID     255

// beware, changing N_CORE_FOR_FWD* SHOULD BE followed by modifying coresponding code
// eg. in hFRPL etc (for histogram counting mechanism)
#define N_CORE_WORKER           15
#define N_CORE_FOR_FWD          15      // maximize core usage
#define N_CORE_FOR_FWD_BITS     0x7FFF00 // NOTE: must match N_CORE_FOR_FWD
#define N_CORE_FOR_HIST_BITS    0x418200 // NOTE: must match the histogram_counting_mechanism

// who will participate in image broadcasting?
#define BCAST_IMG_NUM_CORE      10      // hence, the tiniest image should be 16x16
#define BCAST_IMG_STARTING_CORE 3
#define BCAST_IMG_ENDING_CORE   (BCAST_IMG_STARTING_CORE+BCAST_IMG_NUM_CORE-1)

// Predefined image processing
#define IMG_OP_SOBEL_NO_FILT    1	// will be carried in arg2.low
#define IMG_OP_SOBEL_WITH_FILT  2	// will be carried in arg2.low
#define IMG_OP_LAP_NO_FILT      3	// will be carried in arg2.low
#define IMG_OP_LAP_WITH_FILT    4	// will be carried in arg2.low
#define IMG_NO_FILTERING        0
#define IMG_WITH_FILTERING      1
#define IMG_SOBEL               0
#define IMG_LAPLACE             1


/*----- MCPL communications -----*/
// pixel forwarding, either from chipFwdr or jpeg-dec
// the following are the base value, the lower word should be modified when
// initializing router
#define MCPL_FWD_PIXEL_SOC      0xF3D70000	// start of fwding chunks, payload==chunk seqment
#define MCPL_FWD_PIXEL_DATA     0xF3D80000	// fwd chunk data for a core
#define MCPL_FWD_PIXEL_EOC      0xF3D90000      // end of chunk
#define MCPL_FWD_PIXEL_EOF      0xF3DA0000	// end of transfer, start grayscaling!
#define MCPL_FWD_IMG_INFO       0xF3DB0000      // gateway tells root master vidpro about image info
#define MCPL_FWD_IMG_INFO_MASK  0xFFFF0000      // the lower word can be used for additional information
#define MCPL_FWD_PIXEL_MASK     0xFFFFFFFF	// lower word is used for core-ID

// MCPL for broadcasting image
#define MCPL_BCAST_IMG          0xbca10000      // gateway send this
#define MCPL_BCAST_IMG_ACK      0xbca20000      // vidpro master send this as a reply (ack)
#define MCPL_BCAST_IMG_INFO     0xbca30000      // give info about current line number
#define MCPL_BCAST_IMG_MASK     0xFFFFFFFF      // the low word is for coreID

// MCPL for broadcasting histogram
#define MCPL_BCAST_HIST         0xbca40000      // low word for histogram chunk index
#define MCPL_BCAST_HIST_MASK    0xFFFF0000

// MCPL for histogram chain
#define MCPL_HIST_CHAIN         0xC4A10000      // see histogram_counting_mechanism.png
#define MCPL_HIST_CHAIN_NEXT    0xC4A20000      // continue the chain

// MCPL for local (intrachip) communication
#define MCPL_2LEAD_MASK         0xFFFFFFFF
#define MCPL_2LEAD              0x21ead000      // to LEAD_CORE

/*----- FR communication -----*/
#define FRPL_FWD_PIXEL_SOC      MCPL_FWD_PIXEL_SOC
#define FRPL_IMGBUFIN_ADDR      0x00000001      // gateway leader tells cores to be ready for SOC
#define FRPL_IMGBUFOUT_ADDR     0x00000002      // and fetch the imgBuf address in sdram
#define FRPL_IMG_SIZE           0x00000003      // core 3 to 12 need this information
#define FRPL_RESET_HISTOGRAM    0x00000004
#define FRPL_BCAST_HIST_GRAY    0x00000005      // broadcast histogram and gray image

#define FRPL_BCAST_DISCOVERY    0x00000006

/*----- SDP-related functionalities -----*/
#define SDP_IMG_INFO_PORT       1
#define SDP_RGB_DATA_PORT       2
#define SDP_EOF_DATA_PORT       3
#define SDP_GEN_DATA_PORT       6
#define SDP_INTERNAL_PORT       7

#define SDP_RESULT_IPTAG        1
#define SDP_RESULT_PORT         20001
#define SDP_GENERIC_IPTAG       2       // used, eg. debug info, acknowledge, etc
#define SDP_GENERIC_PORT        20002

#define SDP_CMD_RC_IPTAG_RC     0x80
#define SDP_CMD_RC_NODE_ID_INFO 0x100

#define MAX_PIXEL_IN_CHUNK      88

// related with stdfix.h
#define REAL                        accum
#define REAL_CONST(x)               (x##k)


typedef struct frame_info {
    unsigned short int wImg;
    unsigned short int hImg;
    unsigned int szPixmap;              // pixel size as wImg x hImg
    unsigned char opType;			// 0==sobel, 1==laplace
    unsigned char opFilter;			// 0==no filtering, 1==with filtering
} frame_info_t;

// worker info
typedef struct w_info {
    uint coreID;			// coreID of all workers (max is 17), hold by leadAp
    uchar active;			// in gateway, if on, it participate in gray image bcasting
                                        // in vidpro, if on, it participate in processing
    // per-core info:
    ushort wImg;		// just a copy of frameInfo.wImg
    ushort hImg;		// just a copy of frameInfo.hImg
    ushort startLine;		// this is per core (it differs from blkStart)
    ushort endLine;			// it differs from blkStart
    uchar *myImgBufIn;		// each worker has its own part of imgBufIn
    uchar *dtcmImgBuf;      // for fetching myImgBufIn, the size must be factor of four and >= wImg
    ushort sz_dtcmImgBuf;    // size of dtcmImgBuf
} w_info_t;


#endif
