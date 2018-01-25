/* spinjpeg.c is the main part that works as a master
 * In this file, the main decoder loop is implemented.
 * It will broadcast messages to workers when an intense computation is
 * found.
 *
 * */
#include "mSpinJPEGdec.h"

/*--------------------------- MAIN FUNCTION -----------------------------*/
/*-----------------------------------------------------------------------*/
void c_main()
{
    app_init();

    //spin1_callback_on (MCPL_PACKET_RECEIVED, count_packets, -1);
    spin1_callback_on (DMA_TRANSFER_DONE, hDMA, 0);
    spin1_callback_on(USER_EVENT, hUEvent, 2);

	// then go in the event-loop
    spin1_start (SYNC_NOWAIT);
}
/*-----------------------------------------------------------------------*/







/*--------------------------- HELPER FUNCTION -----------------------------*/
/*-------------------------------------------------------------------------*/

// initIPTag is private here:
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

static void initImgBuf()
{
	// without sdramImgBuf = sark_alloc, it creates WDOG!!!
	sdramImgBuf = sark_alloc(1, sizeof(FILE_t));
	sdramImgBuf->isOpened = 0;
	/* The following might not be necessary:
	sdramImgBuf->nCharRead = 0;
	sdramImgBuf->stream = NULL;
	sdramImgBuf->ptrRead = NULL;
	sdramImgBuf->ptrWrite = NULL;
	sdramImgBuf->szBuffer = 0;
	sdramImgBuf->szFile = 0;
	*/

	// also, ColorBuffer and FrameBuffer might be initialized, so that it
	// is safe to do free_structures()
	ColorBuffer = NULL;
	FrameBuffer = NULL;
}

void initSDPresult()
{
    sdpResult.tag = SDP_SEND_RESULT_TAG;
    sdpResult.flags = 7;
    sdpResult.srce_addr = sv->p2p_addr;
    sdpResult.srce_port = (SDP_PORT_JPEG_INFO << 5) | coreID;
    sdpResult.dest_addr = sv->eth_addr;
    sdpResult.dest_port = PORT_ETH;
}

void app_init ()
{
    coreID = spin1_get_core_id ();
    if(coreID == SDP_RECV_CORE_ID) {
        spin1_callback_on (SDP_PACKET_RX, hSDP, -1);
    }
	if(sark_chip_id()==0)
		initIPTag();
	// TODO: refactor this: sdramImgBufInitialized = false;
	// create a new image buffer
	initImgBuf();

    // for time measurement
    initSDPresult();

    io_printf(IO_STD, "[mSpinJPEGdec] Started with DEBUG_MODE-%d\n",DEBUG_MODE);
}


/* Round up to the next highest power of 2 
 * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2 */
uint roundUp(uint v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

/* so, when closeImgBuf() is called?
 * if this app is for one image, then it can be called after decoding is finish
 * but if it is for video, then it should be called when the last frame is processed
 * HENCE: needs special code sent from PC via SDP
 */
void closeImgBuf()
{
	if(decIsStarted) {
#if(DEBUG_MODE>3)
		io_printf(IO_STD, "Decoding is in progress!\n");
#endif
		return;
	}
	if(sdramImgBuf->isOpened) {
#if(DEBUG_MODE>0)
		io_printf(IO_STD, "Releasing sdramImgBuf!\n");
#endif
		sark_xfree(sv->sdram_heap, sdramImgBuf->stream, ALLOC_LOCK);
		sdramImgBuf->isOpened = false;
	}
}

/* resizeImgBuf() is called when host-PC trigger an SDP with SDP_CMD_INIT_SIZE
 *
 */
void resizeImgBuf(uint szFile, uint portSrc)
{
    bool createNew;
	// we use roundUp() to anticipate the expansion of memory requirement
	// this is because sark doesn't have realloc()
	uint szBuffer = roundUp(szFile);

	if(sdramImgBuf->isOpened == 1) {
		if(szBuffer > sdramImgBuf->szBuffer) {
		   // sdramImgBufSize firstly initialized when createNew
		   sark_xfree(sv->sdram_heap, sdramImgBuf->stream, ALLOC_LOCK);
           createNew = true;
        } else {
           // nothing to change, just
           // reset the image buffer pointer to the starting position
		   // this is the outcome of rounding up mechanism
		   sdramImgBuf->ptrWrite = sdramImgBuf->stream;
		   sdramImgBuf->ptrRead = sdramImgBuf->stream;
		   sdramImgBuf->nCharRead = 0;
		   createNew = false;
#if(DEBUG_MODE > 3)
           io_printf(IO_STD, "No need to create a new sdram buffer\n");
		   io_printf(IO_STD, "sdramImgBuf->ptrWrite is reset to 0x%x\n", sdramImgBuf->ptrWrite);
#endif
        }
    } else {
        createNew = true;
    }

    if(createNew) {
        /* From sark.h regarding sark_xalloc:
           Returns NULL on failure (not enough memory, bad tag or tag in use).

           The flag parameter contains two flags in the bottom two bits. If bit 0
           is set (ALLOC_LOCK), the heap manipulation is done behind a lock with
           interrupts disabled. If bit 1 is set (ALLOC_ID), the block is tagged
           with the AppID provided in bits 15:8 of the flag, otherwise the AppID
           of the current application is used.

           The 8-bit "tag" is combined with the AppID and stored in the "free"
           field while the block is allocated. If the "tag" is non-zero, the
           block is only allocated if the tag is not in use.  The tag (and AppID)
           is stored in the "alloc_tag" table while the block is allocated.

           Hence, flag is set only to ALLOC_LOC: behind lock
                  tag is set to 0, to make sure it is always allocated (using AppID) 
        */

		sdramImgBuf->szFile = szFile;
		sdramImgBuf->szBuffer = szBuffer;

#if(DEBUG_MODE > 3)
		if(portSrc==SDP_PORT_RAW_INFO)
			io_printf(IO_STD, "Allocating %d-bytes of sdram for image %dx%d\n",
					  sdramImgBuf->szBuffer,wImg,hImg);
		else
			io_printf(IO_STD, "Allocating %d-bytes of sdram\n",sdramImgBuf->szBuffer);
#endif

		sdramImgBuf->stream = sark_xalloc (sv->sdram_heap, sdramImgBuf->szBuffer, 0, ALLOC_LOCK);
		if(sdramImgBuf->stream != NULL) {
			sdramImgBuf->ptrWrite = sdramImgBuf->stream;
			sdramImgBuf->ptrRead = sdramImgBuf->stream;
			sdramImgBuf->nCharRead = 0;
			sdramImgBuf->isOpened = 1;
#if(DEBUG_MODE > 3)
			io_printf(IO_STD, "Sdram buffer is allocated at 0x%x with write-ptr at 0x%x\n",
					  sdramImgBuf->stream, sdramImgBuf->ptrWrite);
#endif
        } else {
			io_printf(IO_STD, "[ERR] Fail to create sdramImgBuf!\n");
            // dangerous: terminate then!
            rt_error (RTE_MALLOC);
        }
	}

	// set decoder starting flag
	decIsStarted = false;
}

/* If fails to decode the JPEG file in sdram buffer, abort the operation gracefully */
void aborted_stream()
{
#if(DEBUG_MODE>0)
	io_printf(IO_STD, "[ERROR] Abnormal end of decompression process!\n");
#endif
	free_structures();
	decIsStarted = false;
	success = false;
	if(closeImgBufAfterDecoding) {
		closeImgBuf();
	}
}

/* free_structure() basically only release buffer of ColorBuffer
 * and FrameBuffer. The sdramImgBuf is not release, because it might
 * be used again.
 * Note: ColorBuffer and FrameBuffer are allocated in get_SOF()
 */
void free_structures()
{
    // TODO: 24 Juni, jam 00:04 baru sampai sini
    int i;

	for (i=0; i<4; i++) if (QTvalid[i]) sark_free(QTable[i]);

	if (ColorBuffer != NULL) sark_xfree(sv->sdram_heap, ColorBuffer, ALLOC_LOCK);
	if (FrameBuffer != NULL) sark_xfree(sv->sdram_heap, FrameBuffer, ALLOC_LOCK);

	for (i=0; MCU_valid[i] != -1; i++) sark_free(MCU_buff[i]);
}

/* Ideas for emitDecodeDone:
 * 1. Send to encoder to test encoder: NO, because the encoder is for gray only!
 * 2. Send back to host-PC
 * */
void emitDecodeDone()
{
#if(DEBUG_MODE>0)
	io_printf(IO_STD, "[INFO] Decoding is done!\n");
#endif
#ifndef FOR_PAPER_ICCES
	streamResultToPC();
#endif
}

void streamResultToPC()
{
	int w;
	w = 2 * ceil_div(x_size, 2); /* round to 2 more */
	/*
	for (i=0; i<y_size; i++)
		fwrite(FrameBuffer+n_comp*i*x_size, n_comp, w, fo);
	*/
#if(DEBUG_MODE>0)
	io_printf(IO_STD, "Streaming back to PC...");
#endif
	// prepare sdp
	sdp_msg_t result;
	result.tag = SDP_SEND_RESULT_TAG;
	result.flags = 7;
	result.srce_addr = sv->p2p_addr;
	result.srce_port = (SDP_PORT_RAW_DATA << 5) | coreID;
	result.dest_addr = sv->dbg_addr;
	result.dest_port = PORT_ETH;

	// iterate until all data has been sent
	//int total = y_size * w ;
	int total = y_size * w * 3; // Remember: RGB image!
	int rem = total;
	int sz = 272;
	uchar *ptr = FrameBuffer;
	do {
		sz = sz < rem ? sz : rem;
		result.length = sizeof(sdp_hdr_t) + sz;
		sark_mem_cpy((void *)&result.cmd_rc, ptr, sz);
		spin1_send_sdp_msg(&result, 10);
		spin1_delay_us(SDP_TX_TIMEOUT);
		ptr += sz;
		rem -= sz;
	} while(rem > 0);
	// finally, send EOF
	result.length = sizeof(sdp_hdr_t);
	spin1_send_sdp_msg(&result, 10);
#if(DEBUG_MODE>0)
	io_printf(IO_STD, "done!\n");
#endif
}

/* reportTmeas() sends tmeas via sdp. The tmeas will be put in arg1.
 *
 * */
void reportTmeas(uint t)
{
    sdpResult.cmd_rc = SDP_CMD_REPORT_TMEAS;
    sdpResult.arg1 = tmeas;
    sdpResult.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
    spin1_send_sdp_msg(&sdpResult, 10);
}
