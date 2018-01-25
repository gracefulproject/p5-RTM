#include "mSpinJPEGenc.h"

/************************************************************************************/
/*-------------------------------- FR EVENT ----------------------------------------*/
/*
void hFR (uint key, uint payload)
{
	switch (key) {
	case FRPL_NEW_RAW_IMG_INFO_KEY:
		wImg = payload >> 16;
		hImg = payload & 0xFFFF;
		break;
    case FRPL_NEW_RAW_IMG_ADDR_KEY:
		sdramImgBuf = (uchar *)payload;
		io_printf(IO_BUF, "Got img %dx%d buffered at 0x%x\n", wImg, hImg, payload);
		spin1_schedule_callback(encode, 0, 0, 1);
		break;
	}
}
*/

void hDMA(uint tid, uint tag)
{
	if(tag==DMA_FETCH_IMG_FOR_SDP_TAG)
		dmaImgFromSDRAMdone = true;
}

/* hSDP() is provided only for testing (used alongside testEncoder.py)
 *
 * */
static void getRawImgInfo(sdp_msg_t *msg);
static void getRawImgData(sdp_msg_t *msg);
static void sendImgInfoAck();
void hSDP(uint mBox, uint port)
{
#if(DEBUG_MODE>0)
    //io_printf(IO_BUF, "sdp in at-%d\n", port);
#endif

    sdp_msg_t *msg = (sdp_msg_t *) mBox;

    /*--------------------- Raw File version -----------------------*/
    /* invoke mSpinJPEGenc to encode it */
    if (port == SDP_PORT_RAW_INFO) {
        if (msg->cmd_rc == SDP_CMD_INIT_SIZE) {
            getRawImgInfo(msg);
        }
    }
    else if (port == SDP_PORT_RAW_DATA) {
        getRawImgData(msg);
    }

    spin1_msg_free (msg);
}

void getRawImgInfo(sdp_msg_t *msg)
{

    uint szImgFile = msg->arg1;
    numProcBlock = msg->seq;    // used during ICCES experiment!!!
    wImg = (ushort)msg->arg2;	// for raw image, this info is explicit from host-PC
    hImg = (ushort)msg->arg3;
    spin1_schedule_callback(resizeImgBuf, szImgFile, SDP_PORT_RAW_INFO, 1);
    // Note: inside resizeImgBuf(), some info are printed

    sendImgInfoAck();
}

void getRawImgData(sdp_msg_t *msg)
{
    // assuming msg->length contains correct value ???
    uint len = msg->length - sizeof(sdp_hdr_t);

    // Note: GUI will send "header only" to signify end of image data
    if(len > 0) {
        spin1_dma_transfer(DMA_RAW_IMG_BUF_WRITE, ptrWrite,
                           (void *)&msg->cmd_rc, DMA_WRITE, len);

        // sdramImgBufPtr first initialized in resizeImgBuf()
        ptrWrite += len;
    }
    // end of image data detected
    else {
#if(DEBUG_MODE>3)
        io_printf(IO_STD, "[mSpinJPEGenc] EOF is received!\n");
        //io_printf(IO_STD, "[INFO] %d-chunks for %d-byte data are collected!\n",nReceivedChunk,szImgFile);
        //io_printf(IO_STD, "[INFO] dmaAllocErrCntr = %d\n", dmaAllocErrCntr);
#endif
        // and start encoding
        spin1_schedule_callback(encode, 0, 0, 1);
    }
}

// send an empty sdp message to ackowledge the image info
void sendImgInfoAck()
{
    sdpResult.length = sizeof(sdp_hdr_t);
    spin1_send_sdp_msg(&sdpResult, 10);
#if(DEBUG_MODE>3)
    io_printf(IO_STD, "[mSpinJPEGenc] ack ImgInfo sent!\n");
#endif
}
