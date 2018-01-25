#include "mSpinJPEGdec.h"

/************************************************************************************/
/*------------------------------- SDP EVENT ----------------------------------------*/
/* hSDP - handler for SDP events
 * port-1: host-PC sends jpg data
 * port-2: host-PC sends command, config, etc. related to jpg data
 * port-3: host-PC sends raw image data
 * port-4: host-PC sends command, config, etc. related to raw image data
 */

// private to this section
static void getRawImgInfo(sdp_msg_t *msg);
static void getRawImgData(sdp_msg_t *msg);
inline static void getJPGImgInfo(sdp_msg_t *msg);
static void getJPGImgData(sdp_msg_t *msg);

void hSDP (uint mBox, uint port)
{
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "sdp in at-%d\n", port);
#endif

    sdp_msg_t *msg = (sdp_msg_t *) mBox;

    /*--------------------- JPEG File version -----------------------*/
	if (port == SDP_PORT_JPEG_INFO) {
        if (msg->cmd_rc == SDP_CMD_INIT_SIZE) {
			getJPGImgInfo(msg);
			// set a flag that it waits a signal if the buffer should be cleared
			closeImgBufAfterDecoding = false;
		}
		/* SDP_CMD_CLOSE_IMAGE should be used when the PC receives the
		 * resulting image/frame from this aplx
		 * */
		else if(msg->cmd_rc == SDP_CMD_CLOSE_IMAGE) {
			closeImgBufAfterDecoding = true;
			// if SDP_CMD_CLOSE_IMAGE arrives late, then decIsStarted might be OFF already
			if(!decIsStarted) closeImgBuf();
		}
    }
    else if (port == SDP_PORT_JPEG_DATA) {
		getJPGImgData(msg);
    }

	/*--------------------- Raw File version -----------------------*/
	/* invoke mSpinJPEGenc to encode it */
	else if (port == SDP_PORT_RAW_INFO) {
        if (msg->cmd_rc == SDP_CMD_INIT_SIZE) {
			getRawImgInfo(msg);
        }
    }
    else if (port == SDP_PORT_RAW_DATA) {
		getRawImgData(msg);
    }

    spin1_msg_free (msg);
}



/************************************************************************************/
/*------------------------------- DMA DONE EVENT -----------------------------------*/
void hDMA (uint tid, uint tag)
{
#if(DEBUG_MODE>2)
	io_printf(IO_STD, "[INFO] DMA with tid-%d is done\n", tid);
#endif
#if(DEBUG_MODE==0)
	/* without debuggin, it should go fast: when the first chunk of jpeg file is received,
	 * trigger the main decoder */

	/* Indar: is it really necessary to start decoding now?
    if(tag == DMA_JPG_IMG_BUF_WRITE && nReceivedChunk == 0) {
        spin1_trigger_user_event(UE_START_DECODER, NULL);
    }
	*/
#endif
}



/************************************************************************************/
/*------------------------------- USER EVENT ---------------------------------------*/
void hUEvent(uint eventID, uint arg)
{
    switch(eventID) {
    case UE_START_DECODER:
		spin1_schedule_callback(decode, NULL, NULL, 1);
		//dumpJPG(); --> result is OK!
        break;
    }
}















/************************************************************************************/
/*------------------------------- LOCAL FUNCTIONS ----------------------------------*/

static void getRawImgInfo(sdp_msg_t *msg)
{
	szImgFile = msg->arg1;
	wImg = (ushort)msg->arg2;	// for raw image, this info is explicit from host-PC
	hImg = (ushort)msg->arg3;
	nReceivedChunk = 0;
	spin1_schedule_callback(resizeImgBuf, szImgFile, SDP_PORT_RAW_INFO, 1);
	// Note: inside resizeImgBuf(), some info are printed
}

static void getRawImgData(sdp_msg_t *msg)
{
#if(DEBUG_MODE>0)
		//io_printf(IO_BUF, "Got %d-bytes of SDP data\n", msg->length);
#endif
		// assuming msg->length contains correct value ???
		uint len = msg->length - sizeof(sdp_hdr_t);
		// Note: GUI will send "header only" to signify end of image data
		if(len > 0) {
			uint tid = spin1_dma_transfer(DMA_RAW_IMG_BUF_WRITE, sdramImgBuf->ptrWrite,
							   (void *)&msg->cmd_rc, DMA_WRITE, len);

#if(DEBUG_MODE>=0)
			if(tid == 0) dmaAllocErrCntr++;
#endif
			// sdramImgBufPtr first initialized in resizeImgBuf()
			sdramImgBuf->ptrWrite += len;
			nReceivedChunk++;
		}
		// end of image data detected
		else {
#if(DEBUG_MODE>0)
			io_printf(IO_STD, "[INFO] End-of-data is detected!\n");
			//io_printf(IO_STD, "[INFO] %d-chunks for %d-byte data are collected!\n",nReceivedChunk,szImgFile);
			//io_printf(IO_STD, "[INFO] dmaAllocErrCntr = %d\n", dmaAllocErrCntr);
#endif
			// TODO: notify mSpinJPEGenc to start the encoding process

			// tell stub the location of itcm and dtcm
			uint newRoute = 1 << (ENC_MASTER_CORE+6);			// send to master of encoder
			uint key = FRPL_NEW_RAW_IMG_INFO_KEY;				// a new raw image is availale
			uint payload = ((uint)wImg << 16) | hImg;
			rtr_fr_set(newRoute);								// send to Encoder
			spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
			key = FRPL_NEW_RAW_IMG_ADDR_KEY;
			payload = (uint)sdramImgBuf;
			io_printf(IO_STD, "[INFO] Encoder is informed with img at 0x%x\n", sdramImgBuf);
			spin1_send_fr_packet(key, payload, WITH_PAYLOAD);	// send!
		}
}

// send an empty sdp message to ackowledge the image info
void sendImgInfoAck()
{
    sdpResult.length = sizeof(sdp_hdr_t);
    spin1_send_sdp_msg(&sdpResult, 10);
#if(DEBUG_MODE>0)
    io_printf(IO_STD, "[mSpinJPEGdec] ack ImgInfo sent!\n");
#endif
}

void getJPGImgInfo(sdp_msg_t *msg)
{
#if(DEBUG_MODE>0)
    io_printf(IO_STD, "[mSpinJPEGdec] recv ImgInfo\n");
#endif
	szImgFile = msg->arg1;
	nReceivedChunk = 0;
	spin1_schedule_callback(resizeImgBuf, szImgFile, SDP_PORT_JPEG_INFO, 1);
    sendImgInfoAck();
}

static void getJPGImgData(sdp_msg_t *msg)
{
	// assuming msg->length contains correct value ???
	uint len = msg->length - sizeof(sdp_hdr_t);
	if(len > 0) {
		uint tid = spin1_dma_transfer(DMA_JPG_IMG_BUF_WRITE, sdramImgBuf->ptrWrite,
						   (void *)&msg->cmd_rc, DMA_WRITE, len);

#if(DEBUG_MODE>=0)
		if(tid == 0) dmaAllocErrCntr++;
#endif
		// sdramImgBufPtr first initialized in resizeImgBuf()
		sdramImgBuf->ptrWrite += len;
		nReceivedChunk++;	// for debugging
#if(DEBUG_MODE>3)
		io_printf(IO_BUF, "sdramImgBuf->ptrWrite is at 0x%x\n", sdramImgBuf->ptrWrite);
#endif
	}
	// Note: GUI will send "header only" to signify end of image data
	// end of image data detected
	else {
#if(DEBUG_MODE>0)
		io_printf(IO_STD, "[INFO] End-of-data is detected!\n");
		/*
		io_printf(IO_STD, "[INFO] %d-chunks for %d-byte data are collected!\n",
				  nReceivedChunk,sdramImgBuf->szFile);
		io_printf(IO_STD, "[INFO] dmaAllocErrCntr = %d\n", dmaAllocErrCntr);
		*/
		io_printf(IO_STD, "[INFO] Received %d chunks\n", nReceivedChunk);
#endif
		// TODO: start decoding, it might be triggered already in the first DMA event
		if(!decIsStarted)
			spin1_trigger_user_event(UE_START_DECODER, NULL);
	}
}
