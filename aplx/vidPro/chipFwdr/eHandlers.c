#include "chipFwdr.h"

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
        // if receiving "OK" from scamp
        if(msg->cmd_rc==SDP_CMD_RC_IPTAG_RC) {
            spin1_schedule_callback(initIPtag, 2, msg->arg1, PRIORITY_LOW);
        }
    }

    spin1_msg_free(msg);
}

void hMC(uint key, uint null)
{

}

void hMCPL(uint key, uint payload)
{

}

void hDMA(uint tid, uint tag)
{

}

void hFRPL(uint key, uint payload)
{

}


/*------------------ Functions implementation ------------------*/
static void ackHost(sdp_msg_t *msg)
{
    msg->dest_addr = sv->eth_addr;
    msg->dest_port = msg->srce_port;
    msg->srce_addr = sv->p2p_addr;
    msg->srce_port = (SDP_GEN_DATA_PORT << 5) +  (uchar)coreID;
    msg->tag = SDP_GENERIC_IPTAG;
    msg->length = sizeof(sdp_msg_t); // just header
    spin1_send_sdp_msg(msg, 10);
}

void getImgInfoAndFwd(sdp_msg_t *msg)
{
    // forward this information to LEAD_CORE in chip <2,2> as well
    msg->dest_addr = GATEWAY_CHIP;
    spin1_send_sdp_msg(msg, 10);
    // Note: the sdp is forwarded only to gateway, the gateway
    // will inform vidpro, but not forwarding the whole sdp info

    // NOTE: when LEAD_CORE in gateway receives this fwded sdp,
    //       it should prepare its cores for receiving the streamed pixels

    // then tell host-PC to start streaming
    ackHost(msg);
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
    uint key = MCPL_FWD_PIXEL_SOC | coreID;
    uint payload = *ptrPixel; // send both #px and #seq
    spin1_send_mc_packet(key, payload, WITH_PAYLOAD);

    // send the pixels
    uchar nPkt = msg->cmd_rc * 3 / sizeof(uint);    // for 88 pixels, it produces 66 Pkt
    key = MCPL_FWD_PIXEL_DATA | coreID;
    for(uchar i=0; i<nPkt; i++) {
        ptrPixel++;
        payload = *ptrPixel;
        spin1_send_mc_packet(key, payload, WITH_PAYLOAD);
    }

    // send end of chunk
    key = MCPL_FWD_PIXEL_EOC | coreID;
    spin1_send_mc_packet(key, 0, NO_PAYLOAD);
}

void getEOFandFwd(sdp_msg_t *msg)
{
    // forward this information as an MC (no payload) to LEAD_CORE in the gateway
    uint key = MCPL_FWD_PIXEL_EOF | LEAD_CORE;
    spin1_send_mc_packet(key, 0, NO_PAYLOAD);
    // upon receiving this, gateway should start rgb2gray encoding
}
