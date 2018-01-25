#include "gateway.h"

static void getImgInfoAndFwd(sdp_msg_t *msg);
static void dispatchRGBdata(sdp_msg_t *msg);
static void getEOFandFwd(sdp_msg_t *msg);

void hSDP(uint mBox, uint port)
{
    sdp_msg_t *msg = (sdp_msg_t *)mBox;

    // in the beginning, host should tell us the size of a frame
    if(port==SDP_IMG_INFO_PORT) {
        getImgInfoAndFwd(msg);
    }
    else if(port==SDP_RGB_DATA_PORT) {
        dispatchRGBdata(msg);
    }
    else if(port==SDP_EOF_DATA_PORT) {
        getEOFandFwd(msg);
    }
    else if(port==SDP_INTERNAL_PORT) {
    }

    spin1_msg_free(msg);
}

void hMC(uint key, uint null)
{
    uint base_key = key & 0xFFFF0000;

    // when MCPL_FWD_PIXEL_EOC is received, do rgb2gray
    if(base_key==MCPL_FWD_PIXEL_EOC) {
        spin1_schedule_callback(rgb2gray, 0, 0, PRIORITY_LOW);
    }

    // MCPL_FWD_PIXEL_EOF is forwarded from chipFwdr of jpeg_enc
    // and will be received by LEAD_CORE
    else if(base_key==MCPL_FWD_PIXEL_EOF) {
        spin1_schedule_callback(histAndBcast, 0, 0, PRIORITY_LOW);
    }
}

void hMCPL(uint key, uint payload)
{
    uint base_key = key & 0xFFFF0000;
    //uint pair_key = key & 0xFFFF;

    // the payload of MCPL_FWD_PIXEL_SOC contains:
    //     high_word (sdp.cmd_rc) = number of RGB pixels in the chunk
    //     low_word  (sdp.seq)    = chunk sequence number
    if(base_key==MCPL_FWD_PIXEL_SOC) {
        //pxBuf.pxSeq = payload & 0xFFFF;
        //pxBuf.pxLen = payload >> 16;
        pxBuf.pxSeqLen = payload;
        pxBuf.ptrRGBbuf = pxBuf.RGBbuf; // reset to initial position

    }

    else if(base_key==MCPL_FWD_PIXEL_DATA) {
        *pxBuf.ptrRGBbuf++ = payload;
    }

    else if(base_key==MCPL_HIST_CHAIN) {
        histogram[payload >> 16] += (payload & 0xFFFF);
    }

    else if(base_key==MCPL_HIST_CHAIN_NEXT) {
        // special for core 2, 6, and 13 since these cores receive
        // packets from two sources
        if(++histChainCntr>=2) {
            if(wInfo.coreID==2 | wInfo.coreID==6 || wInfo.coreID==13)
                spin1_schedule_callback(histAndBcast,payload,0,PRIORITY_LOW);
        }
        else {
            spin1_schedule_callback(histAndBcast,payload,0,PRIORITY_LOW);
        }
    }

    else if(base_key==MCPL_BCAST_IMG_ACK) {
        mcpl_bcastGrayImg_cont=TRUE;
    }
}

void hDMA(uint tid, uint tag)
{
    if(tag & 0xFFFF == DMA_TAG_BCAST_IMG) {
        dma_bcastGrayImg_cont = TRUE;
    }
}

void hFRPL(uint key, uint payload)
{
    if(key==FRPL_IMGBUFIN_ADDR) {
        imgBufIn = (uchar *)payload;
    }
    else if(key==FRPL_IMGBUFOUT_ADDR) {
        imgBufOut = (uchar *)payload;
    }
    else if(key==FRPL_IMG_SIZE) {
        wInfo.wImg = payload >> 16;
        wInfo.hImg = payload & 0xFFFF;
        spin1_schedule_callback(computeWLoad,0,0,PRIORITY_LOW);
    }
}

void hFR(uint key, uint null)
{
    // FRPL_RESET_HISTOGRAM will be sent to all working cores
    // during initImgBuf(). This happens everytime sdp with SDP_IMG_INFO_PORT
    // is received by LEAD_CORE
    if(key==FRPL_RESET_HISTOGRAM) {
        resetHistogram();
    }
    else if(key==FRPL_BCAST_HIST_GRAY) {
        if(wInfo.coreID==LEAD_CORE) {
            spin1_schedule_callback(bcastHistogram,0,0,PRIORITY_LOW);
        }
        else {
            // in the bcastGrayImg, it should be checked if the core is active one
            spin1_schedule_callback(bcastGrayImg,0,0,PRIORITY_LOW);
        }
    }
}

// bcastFR() is used mainly be LEAD_CORE to tell all working cores something
inline void bcastFR(uint key, uint payload, uint load_condition)
{
    rtr_fr_set(N_CORE_FOR_FWD_BITS);
    spin1_send_fr_packet(key, payload, load_condition);
}

/*------------------ Functions implementation ------------------*/
void getImgInfoAndFwd(sdp_msg_t *msg)
{

    // NOTE: when LEAD_CORE in gateway receives this fwded sdp,
    //       it should prepare its cores for receiving the streamed pixels
    //       --> it's done during initImgBuf()

    // and collect info for internal purpose
    frameInfo.wImg = msg->arg1 >> 16;
    frameInfo.hImg = msg->arg1 & 0xFFFF;
    //frameInfo.szPixmap = frameInfo.wImg * frameInfo.hImg * 3; // for RGB, but not needed
    frameInfo.szPixmap = frameInfo.wImg * frameInfo.hImg; // this is for gray only buffer

    // the following needs to be forwarded to vidpro:
    switch(msg->seq & 0xFF) {
    case IMG_OP_SOBEL_NO_FILT:
        frameInfo.opFilter = IMG_NO_FILTERING; frameInfo.opType = IMG_SOBEL; break;
    case IMG_OP_SOBEL_WITH_FILT:
        frameInfo.opFilter = IMG_WITH_FILTERING; frameInfo.opType = IMG_SOBEL; break;
    case IMG_OP_LAP_NO_FILT:
        frameInfo.opFilter = IMG_NO_FILTERING; frameInfo.opType = IMG_LAPLACE; break;
    case IMG_OP_LAP_WITH_FILT:
        frameInfo.opFilter = IMG_WITH_FILTERING; frameInfo.opType = IMG_LAPLACE; break;
    }
    //next, broadcast the above information to vidpro!
    spin1_send_mc_packet(MCPL_FWD_IMG_INFO | msg->seq, msg->arg1, WITH_PAYLOAD;)

    // also, inform workers that broadcast gray image about the size of the frame
    bcastFR(FRPL_IMG_SIZE, msg->arg1, WITH_PAYLOAD);

    // then schedule for image buffer creation
    spin1_schedule_callback(initImgBuf, 0, 0, PRIORITY_LOW);
}

/* dispatchRGBdata forwards pixel chunk to gateway
 * the structure of data in sdp:
 *     cmd_rc = number of RGB pixels in the chunk
 *     seq = chunk sequence number
 *     arg1,arg2,arg3,data[252] = pixel chunk
 * */
void dispatchRGBdata(sdp_msg_t *msg)
{
    // send start of chunk message
    uint *ptrPixel = (uint *)&msg->cmd_rc;
    uint key = MCPL_FWD_PIXEL_SOC | wInfo.coreID;
    uint payload = *ptrPixel; // send both #px and #seq
    spin1_send_mc_packet(key, payload, WITH_PAYLOAD);
    uchar nPixel = msg->cmd_rc;
    key = MCPL_FWD_PIXEL_DATA | wInfo.coreID;
    for(uchar i=0; i<nPixel; i++) {
        ptrPixel++;
        payload = *ptrPixel;
        spin1_send_mc_packet(key, payload, WITH_PAYLOAD);
    }
}

void getEOFandFwd(sdp_msg_t *msg)
{
    // forward this information as an MC (no payload) to LEAD_CORE in the gateway
    uint key = MCPL_FWD_PIXEL_EOF | wInfo.coreID;
    spin1_send_mc_packet(key, 0, NO_PAYLOAD);
    // upon receiving this, gateway should start rgb2gray encoding
}
