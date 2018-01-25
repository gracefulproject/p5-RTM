#include "mSpinJPEGenc.h"

#define ENABLE_TIMER() tc[T2_CONTROL] = 0x82
#define START_TIMER() tc[T2_LOAD] = 0
#define READ_TIMER() ((0 - tc[T2_COUNT]) / sark.cpu_clk)

/*--------------------------- MAIN FUNCTION -----------------------------*/
/*-----------------------------------------------------------------------*/
void c_main()
{
    app_init();

    //spin1_callback_on (MCPL_PACKET_RECEIVED, count_packets, -1);
	spin1_callback_on (DMA_TRANSFER_DONE, hDMA, -1);
    //spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, 0);
    spin1_callback_on(SDP_PACKET_RX, hSDP, 1);

	// then go in the event-loop
    spin1_start (SYNC_NOWAIT);
}
/*-----------------------------------------------------------------------*/

/* encode() is the main event loop which is called when a new image arrives
 * */
void encode(uint arg0, uint arg1)
{
	uint time;
	START_TIMER ();	// Start measuring
#if(DEBUG_MODE>0)
    io_printf(IO_STD, "[mSpinJPEGenc] Start encoding...\n");
#endif
	e = jpec_enc_new(sdramImgBuf, wImg, hImg, jpgQ);
	jpgResult = jpec_enc_run(e, &szjpgResult);
	time = READ_TIMER ();	// Get elapsed time
#ifndef FOR_PAPER_ICCES
    sendJPG((uint)jpgResult, (uint)szjpgResult);
#else
    io_printf(IO_STD, "t = %d-us\n", time);
    sendResult(time);
#endif
	jpec_enc_del(e);
}


void sendResult(uint tmeas)
{
    sdpResult.arg1 = tmeas;
    sdpResult.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
    spin1_send_sdp_msg(&sdpResult, 10);
}







/*------------------------ RASTER MAIN FUNCTION ---------------------------*/
/*-------------------------------------------------------------------------*/
#ifndef FOR_PAPER_ICCES
void sendJPG(uint imgAddr, uint imgSize)
{
	uint dmaLen, sdpLen;
	uint rem = imgSize;
	uint sdpRem;	// remaining data to send

	// initialize the dma request size
	dmaLen = 6800; // 6800 == (256+16)*25 == 272*25; --> for 25 stream of sdp

	// first, allocate dtcmImgBuf and its ptr
	dtcmImgBuf = sark_alloc(1, dmaLen);
	uchar *imgPtr = (uchar *)imgAddr;
	uchar *ptr;
	uint cntr = 0;
#if(DEBUG_MODE>2)
	io_printf(IO_STD, "[mSpinJPEGenc] Streming %d-bytes data!\n", imgSize);
#endif
	do {
		dmaLen = dmaLen < rem ? dmaLen : rem;
		// fetch data from sdram
		dmaImgFromSDRAMdone = false;
#if(DEBUG_MODE>1)
		/*
		io_printf(IO_STD, "[sendJPG] Fetching at 0x%x for %d-bytes\n",
				  imgPtr, dmaLen);
		*/
#endif
		spin1_dma_transfer(DMA_FETCH_IMG_FOR_SDP_TAG, (void *)imgPtr,
						   (void *)dtcmImgBuf, DMA_READ, dmaLen);
		while(!dmaImgFromSDRAMdone) {
		}
		// then split into chunks and send the sdp
		ptr = dtcmImgBuf;
		// initialize sdp size
		sdpLen = 272;
		sdpRem = dmaLen;
		do {
			sdpLen = sdpLen < sdpRem ? sdpLen : sdpRem;
			sark_mem_cpy((void *)&sdpResult.cmd_rc, (void *)ptr, sdpLen);
			sdpResult.length = sizeof(sdp_hdr_t) + sdpLen;
			spin1_send_sdp_msg(&sdpResult, 10);
			ptr += sdpLen;
			sdpRem -= sdpLen;
#if(DEBUG_MODE>1)
			//spin1_delay_us(50000);
			spin1_delay_us(SDP_TX_TIMEOUT);
			cntr++;
			/*
			io_printf(IO_STD, "[sendJPG] Chunk-%d len=%d, sdpRem=%d\n",
					  cntr, sdpResult.length, sdpRem);
			*/
#else
			spin1_delay_us(SDP_TX_TIMEOUT); // otherwise, many sdp packet get dropped
#endif
		} while( sdpRem > 0);
		imgPtr += dmaLen;
		rem -= dmaLen;
	} while(rem > 0);
	// then release dtcmImgBuf
	sark_free(dtcmImgBuf);
	// finally, inform host that transmission is complete
#if(DEBUG_MODE>1)
	io_printf(IO_STD, "[sendJPG] EOF!\n");
#endif
	sdpResult.length = sizeof(sdp_hdr_t);	// empty message == EOF
	spin1_send_sdp_msg(&sdpResult, 10);
}
#endif

/* resizeImgBuf() is called when host-PC trigger an SDP with SDP_CMD_INIT_SIZE
 *
 */
void resizeImgBuf(uint szFile, uint portSrc)
{
    if(sdramImgBuf!=NULL) {
        sark_xfree(sv->sdram_heap, sdramImgBuf, ALLOC_LOCK);
    }

    sdramImgBuf = sark_xalloc (sv->sdram_heap, szFile, 0, ALLOC_LOCK);
    if(sdramImgBuf == NULL) {
        io_printf(IO_STD, "[ERR] Fail to create sdramImgBuf!\n");
        // dangerous: terminate then!
        rt_error (RTE_MALLOC);
    } else {
        ptrWrite = sdramImgBuf;
    }
}









/*--------------------------- HELPER FUNCTION -----------------------------*/
/*-------------------------------------------------------------------------*/
/*--- private function ---*/
static void initIPTag()
{
	// make sure ip tag SDP_SEND_RESULT_TAG is set correctly
	sdp_msg_t iptag;
	iptag.flags = 0x07;	// no replay
	iptag.tag = 0;		// internal
	iptag.srce_addr = sv->p2p_addr;
	iptag.srce_port = 0xE0 + coreID;	// use port-7
	iptag.dest_addr = sv->p2p_addr;
	iptag.dest_port = 0;				// send to "root"
	iptag.cmd_rc = 26;
	// set the reply tag
	iptag.arg1 = (1 << 16) + SDP_SEND_RESULT_TAG;
	iptag.arg2 = SDP_SEND_RESULT_PORT;
	iptag.arg3 = SDP_HOST_IP;
	iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
	spin1_send_sdp_msg(&iptag, 10);
}

void app_init ()
{
    coreID = spin1_get_core_id();
	// prepare sdp for data
	sdpResult.flags = 7;	// no reply
    sdpResult.tag = SDP_SEND_RESULT_TAG;
	sdpResult.srce_addr = sv->p2p_addr;
	sdpResult.srce_port = (SDP_PORT_RAW_DATA << 5) | coreID;
	sdpResult.dest_addr = sv->dbg_addr;
	sdpResult.dest_port = PORT_ETH;
	// prepare IP Tag
	initIPTag();
#ifdef FOR_PAPER_ICCES
    io_printf(IO_STD, "[mSpinJPEGenc] Started in DEBUG_MODE-%d for ICCES\n", DEBUG_MODE);
#else
    io_printf(IO_STD, "[mSpinJPEGenc] Started in DEBUG_MODE-%d\n", DEBUG_MODE);
#endif
	ENABLE_TIMER ();	// Enable timer (once)
}
