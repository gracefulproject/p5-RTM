#include "SpiNNEdge.h"

/* TODO:
 * - We can use hTimer for pooling workers from time to time (to find out,
 *   which one is alive).
 * */
void hTimer(uint tick, uint Unused)
{
	// first-tick: populate workers
	// don't start at tick-0 !!!!
	if(tick==1) {
		sark_delay_us(1000*sv->p2p_addr);
        io_printf(IO_BUF, "[SpiNNEdge] Collecting worker IDs...\n");
		/* Initial process: broadcast info:
		 * payload == 0 : ping
		 * payload != 0 : location of block_info_t variable
		 * */
		spin1_send_mc_packet(MCPL_BCAST_INFO_KEY, 0, WITH_PAYLOAD);
		spin1_send_mc_packet(MCPL_BCAST_INFO_KEY, (uint)blkInfo, WITH_PAYLOAD);
	}
	// second tick: broadcast info to workers, assuming 1-sec is enough for ID collection
	else if(tick==2) {
        //sark_delay_us(1000*sv->p2p_addr);
        io_printf(IO_BUF, "[SpiNNEdge] Distributing wIDs...\n");
		// payload.high = tAvailable, payload.low = wID
		for(uint i= 1; i<workers.tAvailable; i++)
			spin1_send_mc_packet(workers.wID[i], (workers.tAvailable << 16) + i, WITH_PAYLOAD);
	}
	else if(tick==3) {
		// debugging
		printWID(0,0);
        //yang berikut akan menghasilkan "Chip-56320 ready!" --> Chip-56320 ???
        //io_printf(IO_STD, "Chip-%d ready!\n", blkInfo->nodeBlockID+1);
#ifdef USE_SPIN5
		io_printf(IO_STD, "[SpiNN5-Edge-v%d.%d] Chip<%d,%d> ready! LeadAp running @ core-%d\n",
				  MAJOR_VERSION, MINOR_VERSION, CHIP_X(sv->p2p_addr), CHIP_Y(sv->p2p_addr),
				  sark_core_id());
#else
		io_printf(IO_STD, "[SpiNN3-Edge-v%d.%d] Chip<%d,%d> ready! LeadAp running @ core-%d\n",
				  MAJOR_VERSION, MINOR_VERSION, CHIP_X(sv->p2p_addr), CHIP_Y(sv->p2p_addr),
				  sark_core_id());
#endif
    }
	else {
        //io_printf(IO_STD, "sv->clock_ms = %u\n", sv->clock_ms);
	}
}

// TODO: initIPTag()
void initIPTag()
{
	// only chip <0,0>
	if(sv->p2p_addr==0) {
		sdp_msg_t iptag;
		iptag.flags = 0x07;	// no replay
		iptag.tag = 0;		// internal
		iptag.srce_addr = sv->p2p_addr;
		iptag.srce_port = 0xE0 + myCoreID;	// use port-7
		iptag.dest_addr = sv->p2p_addr;
		iptag.dest_port = 0;				// send to "root"
		iptag.cmd_rc = 26;
		// set the reply tag
		iptag.arg1 = (1 << 16) + SDP_TAG_REPLY;
		iptag.arg2 = SDP_UDP_REPLY_PORT;
		iptag.arg3 = SDP_HOST_IP;
		iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
		spin1_send_sdp_msg(&iptag, 10);
		// set the result tag
		iptag.arg1 = (1 << 16) + SDP_TAG_RESULT;
		iptag.arg2 = SDP_UDP_RESULT_PORT;
		spin1_send_sdp_msg(&iptag, 10);
        // set the debug tag
        iptag.arg1 = (1 << 16) + SDP_TAG_DEBUG;
        iptag.arg2 = SDP_UDP_DEBUG_PORT;
        spin1_send_sdp_msg(&iptag, 10);
    }
}

void sendReply(uint arg0, uint arg1)
{
	// io_printf(IO_BUF, "Sending reply...\n");
	reportMsg.cmd_rc = SDP_CMD_REPLY_HOST_SEND_IMG;
	reportMsg.seq = arg0;
	reportMsg.dest_addr = sv->eth_addr;
	reportMsg.dest_port = PORT_ETH;
	//reportMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
    spin1_send_sdp_msg(&reportMsg, 10);
	//spin1_delay_us(50);
	//if(checkSDP == 0)
	//	io_printf(IO_STD, "SDP fail!\n");
}

// since R,G and B channel processing are similar
void getImage(sdp_msg_t *msg, uint port)
{
	// if I'm not include in the list, skip this
	if(blkInfo->maxBlock==0) return;

	uint checkDMA = 0;
	uchar *imgIn;
	switch(port) {
	case SDP_PORT_R_IMG_DATA: imgIn = blkInfo->imgRIn; break;	// blkInfo->imgRIn is altered in hDMA!!!
	case SDP_PORT_G_IMG_DATA: imgIn = blkInfo->imgGIn; break;
	case SDP_PORT_B_IMG_DATA: imgIn = blkInfo->imgBIn; break;
	}

    // get the image data from SCP+data_part
    dLen = msg->length - sizeof(sdp_hdr_t);	// dLen is used by hDMADone()
    // io_printf(IO_BUF, "Receiving %d-bytes\n", dLen);

#ifndef USE_MCPL_FOR_FWD
    // forward?
	if(chainMode == 1 && sark_chip_id()==0) {
        //io_printf(IO_BUF, "Forwarding to node ");
        for(ushort i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
            //io_printf(IO_BUF, "%d ", chips[i].id);
            msg->dest_addr = (chips[i].x << 8) + chips[i].y;
            //msg->srce_addr = sv->p2p_addr;	// replace with my ID instead of ETH
            //msg->srce_port = myCoreID;
            uint checkSDP = spin1_send_sdp_msg(msg, 10);
            //if(checkSDP==0)
            //	io_printf(IO_STD, "SDP fail to sent!\n");
        }
        //io_printf(IO_BUF, "\n");
	}
#endif

    // the processing of chip<0,0>, the other chips depends on MCPL
	if(dLen > 0) {

        //io_printf(IO_BUF, "blkInfo->imgRIn = 0x%x\n", blkInfo->imgRIn);
		// in the beginning, dtcmImgBuf is NULL
		/*
		if(dtcmImgBuf==NULL) {
			io_printf(IO_BUF, "Allocating dtcmImgBuf...\n");
			dtcmImgBuf = sark_alloc(sizeof(cmd_hdr_t) + SDP_BUF_SIZE, sizeof(uchar));
			if(dtcmImgBuf==NULL) {
				io_printf(IO_STD, "dtcmImgBuf alloc fail!\n");
				rt_error(RTE_ABORT);
			}
			dmaImg2SDRAMdone = 1;	// will be set in hDMA()
		}
		*/
		whichRGB = port;

		// Indar: kenapa blok-1 kacau? coba copy ke pixelBuffer, bukan ke dtcmImgBuf
		// spin1_memcpy((void *)dtcmImgBuf, (void *)&msg->cmd_rc, dLen);
		spin1_memcpy((void *)pixelBuffer, (void *)&msg->cmd_rc, dLen);
#ifdef USE_MCPL_FOR_FWD
        // forward in MCPL mode
		//uint *ptrBuf = (uint *)dtcmImgBuf;
		uint buf;
        uint t = dLen / sizeof(uint);
        if(dLen % sizeof(uint) > 0) t++;

		uint key;
        //Q: how dLen will be transmitted? it is needed in hDMA by node other than <0,0>
        for(uint px=0; px<t; px++) {
			//sark_mem_cpy((void *)&buf, (void *)dtcmImgBuf+px*sizeof(uint), sizeof(uint));
			buf = pixelBuffer[px];
			key = MCPL_BCAST_PIXEL_BASE + (px << 4);
            switch(port) {
            case SDP_PORT_R_IMG_DATA:
				key += 1; break;
            case SDP_PORT_G_IMG_DATA:
				key += 3; break;
            case SDP_PORT_B_IMG_DATA:
				key += 5;
			}
			spin1_send_mc_packet(key, buf, WITH_PAYLOAD);
			//spin1_send_mc_packet(key, *ptrBuf, WITH_PAYLOAD);
			//io_printf(IO_STD, "MCPL_BCAST_*pixel-%d: key=0x%x, data=0x%x\n", px, key,*ptrBuf);
			//ptrBuf++;
			//sark_delay_us(4000000);
        }
		/*
        switch(port) {
        case SDP_PORT_R_IMG_DATA:
            spin1_send_mc_packet(MCPL_BCAST_RED_PX_END, dLen, WITH_PAYLOAD); break;
        case SDP_PORT_G_IMG_DATA:
            spin1_send_mc_packet(MCPL_BCAST_GREEN_PX_END,dLen, WITH_PAYLOAD); break;
        case SDP_PORT_B_IMG_DATA:
            spin1_send_mc_packet(MCPL_BCAST_BLUE_PX_END, dLen, WITH_PAYLOAD);
        }
		*/
		key += 1;
		spin1_send_mc_packet(key, dLen, WITH_PAYLOAD);
		//io_printf(IO_STD, "MCPL_BCAST_*END key-0x%x...\n", key);
#endif
		// copy to dtcm buffer, if the previous transfer is complete
		// spin1_memcpy((void *)imgRIn, (void *)&msg->cmd_rc, dLen);
		dmaImg2SDRAMdone = 0;	// reset the dma done flag
		do {

			// Indar: coba pakai pixelBuffer, bukan dtcmImgBuf
			//checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG , (void *)imgIn,
			//							  (void *)dtcmImgBuf, DMA_WRITE, dLen);
			checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG , (void *)imgIn,
										  (void *)pixelBuffer, DMA_WRITE, dLen);

		} while(checkDMA==0);
		// imgRIn += dLen; -> moved to hDMADone

		// if the previous dma transfer is still in progress...
		// while(dmaImg2SDRAMdone==0) {
		// }

		// send reply immediately only if the sender is host-PC
		// if(msg->srce_port == PORT_ETH)	// no, because we have modified srce_port in chainMode 1
		if(chainMode == 0 || (chainMode == 1 && sv->p2p_addr == 0)) {
			// io_printf(IO_STD, "Replied!\n");
			sendReply(dLen, 0);
		}

	}
	// if zero data is sent, means the end of message transfer!
	else {
#ifdef USE_MCPL_FOR_FWD
        // tell the other nodes to start processing
		// io_printf(IO_BUF, "Broadcasting MCPL_BCAST_IMG_READY\n");
        spin1_send_mc_packet(MCPL_BCAST_IMG_READY, port, WITH_PAYLOAD);
#endif
        afterCollectPixel(port, 0);
    }
}

// collectPixel() is basically similar to getImage, but it is for other nodes than <0,0>
// it is called when
// MCPL_BCAST_BLUE_PX_END or MCPL_BCAST_GREEN_PX_END or MCPL_BCAST_RED_PX_END is received
void collectPixel(uint key, uint dataLength)
{
    // reset the counter for the next delivery
    pixelCntr = 0;

    //io_printf(IO_BUF, "pkt-%d\n", dataLength);
    uint checkDMA;
    uchar *imgIn;
	switch(key & 0xF) {
	// case MCPL_BCAST_RED_PX_END:
	case 2:
        whichRGB = SDP_PORT_R_IMG_DATA;
        imgIn = blkInfo->imgRIn;
        break;
	// case MCPL_BCAST_GREEN_PX_END:
	case 4:
        whichRGB = SDP_PORT_G_IMG_DATA;
        imgIn = blkInfo->imgGIn;
        break;
	// case MCPL_BCAST_BLUE_PX_END:
	case 6:
        whichRGB = SDP_PORT_B_IMG_DATA;
        imgIn = blkInfo->imgBIn;
        break;
    }

    // Note: blkInfo->imgRIn is altered in hDMA!!!
	// it needs dLen to be supplied beforehand !!!
    dLen = dataLength;
    checkDMA = spin1_dma_transfer(DMA_STORE_IMG_TAG , (void *)imgIn,
                                  (void *)pixelBuffer, DMA_WRITE, dataLength);

}

void afterCollectPixel(uint port, uint Unused)
{
	// if I'm not include in the list, skip this
	if(blkInfo->maxBlock==0) return;
	// who am i?
	//io_printf(IO_BUF, "p2p_addr = 0x%x\n", sv->p2p_addr);
	// io_printf(IO_BUF, "After collect\n");
    //if(dtcmImgBuf != NULL) {

	/* Indar: apa perlu dtcmImgBuf di reset? karena tidak dipakai disini
    if(sv->p2p_addr = 0) {
        sark_free(dtcmImgBuf);
        dtcmImgBuf = NULL;	// reset ImgBuffer in DTCM
    }
	*/
    switch(port) {
    case SDP_PORT_R_IMG_DATA:
		//io_printf(IO_BUF, "layer-R is complete!\n");
        blkInfo->imgRIn = (char *)IMG_R_BUFF0_BASE;	// reset to initial base position
        blkInfo->fullRImageRetrieved = 1;
        break;
    case SDP_PORT_G_IMG_DATA:
		//io_printf(IO_BUF, "layer-G is complete!\n");
        blkInfo->imgGIn = (char *)IMG_G_BUFF0_BASE;	// reset to initial base position
        blkInfo->fullGImageRetrieved = 1;
        break;
    case SDP_PORT_B_IMG_DATA:
		//io_printf(IO_BUF, "layer-B is complete!\n");
        blkInfo->imgBIn = (char *)IMG_B_BUFF0_BASE;	// reset to initial base position
        blkInfo->fullBImageRetrieved = 1;
        break;
    }


    // if grey or at the end of B image transmission, it should trigger processing
    if(blkInfo->isGrey==1 || port==SDP_PORT_B_IMG_DATA) {
        if(sv->p2p_addr==0) {
			// send empty sdp just to notify host that spinnaker is starting to process
            tic = sv->clock_ms;
			// let's see with timer 2
			START_TIMER ();	// Start measuring
			/*
			io_printf(IO_STD, "Full image is retrieved! Start processing at %u by chip-%d!\n",
					  tic, sv->p2p_addr);
			*/
            resultMsg.length = sizeof(sdp_hdr_t);   // send empty message
            resultMsg.srce_port = myCoreID;
            resultMsg.srce_addr = sv->p2p_addr;
            spin1_send_sdp_msg(&resultMsg, 10);
        }
		// Indar: disable this to coba lihat apa konten memori sudah benar
		spin1_schedule_callback(triggerProcessing, 0, 0, PRIORITY_PROCESSING);
    }
}

/*
// Let's use:
// port-7 for receiving configuration and command:
//		cmd_rc == SDP_CMD_CONFIG -->
//			arg1.high == img_width, arg1.low == img_height
//			arg2.high : 0==gray, 1==colour(rgb)
//			seq.high == isGray: 1==gray, 0==colour(rgb)
//			seq.low == type_of_operation:
//						1 = sobel_no_filtering
//						2 = sobel_with_filtering
//						3 = laplace_no_filtering
//						4 = laplace_with_filtering
//		cmd_rc == SDP_CMD_PROCESS
//		cmd_rc == SDP_CMD_CLEAR
If leadAp detects filtering, first it ask workers to do filtering only.
Then each worker will start filtering. Once finished, each worker
will report to leadAp about this filtering.
Once leadAp received filtering report from all workers, leadAp
trigger the edge detection
*/
void hSDP(uint mBox, uint port)
{

	sdp_msg_t *msg = (sdp_msg_t *)mBox;
	/* Hasil: jadi tahu, QDataStream secara default pakai big endian
	io_printf(IO_STD, "Debugging streamer...\n");
	io_printf(IO_STD, "port = %d\n", port);
	io_printf(IO_STD, "cmd_rc = 0x%x\n", msg->cmd_rc);
	io_printf(IO_STD, "seq = 0x%x\n", msg->seq);
	io_printf(IO_STD, "arg1 = 0x%x\n", msg->arg1);
	io_printf(IO_STD, "arg2 = 0x%x\n", msg->arg2);
	io_printf(IO_STD, "arg2 = 0x%x\n", msg->arg3);
	*/

	// if host send SDP_CONFIG, means the image has been
	// loaded into sdram via ybug operation
	/*
	if(msg->srce_addr==0)
		io_printf(IO_BUF, "Got a forwarded packet from root\n");
	*/
	if(port==SDP_PORT_CONFIG) {
		if(msg->cmd_rc == SDP_CMD_CONFIG || msg->cmd_rc == SDP_CMD_CONFIG_CHAIN) {
			blkInfo->wImg = msg->arg1 >> 16;
			blkInfo->hImg = msg->arg1 & 0xFFFF;
			blkInfo->nodeBlockID = msg->arg2 >> 16;
			blkInfo->maxBlock = msg->arg2 & 0xFFFF;
			blkInfo->isGrey = msg->seq >> 8; //1=Grey, 0=color
			switch(msg->seq & 0xFF) {
			case IMG_OP_SOBEL_NO_FILT:
				blkInfo->opFilter = IMG_NO_FILTERING; blkInfo->opType = IMG_SOBEL;
				break;
			case IMG_OP_SOBEL_WITH_FILT:
				blkInfo->opFilter = IMG_WITH_FILTERING; blkInfo->opType = IMG_SOBEL;
				break;
			case IMG_OP_LAP_NO_FILT:
				blkInfo->opFilter = IMG_NO_FILTERING; blkInfo->opType = IMG_LAPLACE;
				break;
			case IMG_OP_LAP_WITH_FILT:
				blkInfo->opFilter = IMG_WITH_FILTERING; blkInfo->opType = IMG_LAPLACE;
				break;
			}
			// info about chain is only useful for root, no need to be forwarded
			if(msg->cmd_rc == SDP_CMD_CONFIG_CHAIN)
				chainMode = 1;
			else
				chainMode = 0;
			// is it use chain mechanism?
			if(sark_chip_id() == 0 && chainMode==1) {
				ushort i, x, y, id, maxBlock = blkInfo->maxBlock;
				/*
				if(chips != NULL)
					sark_free(chips);
				chips = sark_alloc(maxBlock-1, sizeof(chain_t));
				*/

#ifdef USE_MCPL_FOR_FWD
				// broadcast image preparation
				spin1_send_mc_packet(MCPL_BCAST_IMG_INFO_BASE, 0, WITH_PAYLOAD);
				// forward node info
				for(i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					x = msg->data[i*3];			chips[i].x = x;
					y = msg->data[i*3 + 1];		chips[i].y = y;
					id = msg->data[i*3 + 2];	chips[i].id = id;
					spin1_send_mc_packet(MCPL_BCAST_IMG_INFO_NODE + (id << 4),
										 (x << 16)+y, WITH_PAYLOAD);

				}

                // forward image size and max block
				spin1_send_mc_packet(MCPL_BCAST_IMG_INFO_SIZE + (maxBlock << 4),
									 msg->arg1, WITH_PAYLOAD);
                //io_printf(IO_STD, "Broadcasting maxBlock and arg-1 = 0x%x\n", msg->arg1);
                //spin1_delay_us(10000000);

                // forward operation and needSendDebug flag
                spin1_send_mc_packet(MCPL_BCAST_IMG_INFO_OPR + (msg->arg3 << 4),
									 msg->seq, WITH_PAYLOAD);
                //io_printf(IO_STD, "Broadcasting seq = 0x%x\n", msg->seq);
                //spin1_delay_us(5000000);

				// finally send notification end of info
				spin1_send_mc_packet(MCPL_BCAST_IMG_INFO_EOF, 0, WITH_PAYLOAD);

#else
				for(i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					x = msg->data[i*3];			chips[i].x = x;
					y = msg->data[i*3 + 1];		chips[i].y = y;
					id = msg->data[i*3 + 2];	chips[i].id = id;

					msg->dest_addr = (x << 8) + y;
					msg->arg2 = (id << 16) + maxBlock;
					msg->srce_addr = sv->p2p_addr;
					msg->srce_port = myCoreID;
					spin1_send_sdp_msg(msg, 10);
					//Note: kalau tidak di-delay dengan io_print di bawah ini,
					//		akan ada missing sdp. Ini kok mirip dengan kasus sendSDP
					//		yang tidak bisa cepat-cepat ya?
					io_printf(IO_BUF, "Forwarding config to chip<%d,%d> as node-%d\n",
							  x,y,id);
				}
#endif

			}

			spin1_schedule_callback(initNode, 0, 0, PRIORITY_PROCESSING);
		}
		// TODO: don't forget to give a "kick" from python?
		else if(msg->cmd_rc == SDP_CMD_CLEAR) {
			if(sv->p2p_addr==0)
				io_printf(IO_STD, "Clearing...\n");
			else
				io_printf(IO_BUF, "Clearning...\n");
			initImage();

			// should be propagated?
			if(sark_chip_id() == 0 && chainMode==1) {
				for(ushort i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					msg->dest_addr = (chips[i].x << 8) + chips[i].y;
					spin1_send_sdp_msg(msg, 10);
				}

			}
		}
		/*
		else if(msg->cmd_rc = SDP_CMD_ACK_RESULT) {
			io_printf(IO_BUF, "Got SDP_CMD_ACK_RESULT with seq=%d for myBlkID=%d\n",
					  msg->seq, blkInfo->nodeBlockID);
			if(msg->seq==blkInfo->nodeBlockID+1) {
				hostAck = 1;
				ackCntr++;
			}
			//spin1_send_mc_packet(MCPL_BCAST_HOST_ACK, msg->seq, WITH_PAYLOAD);
			/*
			// propagate?
			if(sark_chip_id() == 0 && chainMode==1) {
				for(ushort i=0; i<blkInfo->maxBlock-1; i++) {	// don't include me!
					msg->dest_addr = (chips[i].x << 8) + chips[i].y;
					spin1_send_sdp_msg(msg, 10);
				}
			}
		}
			*/
	}

    else if(port==SDP_PORT_ACK) {
        //io_printf(IO_STD, "Got ack. Send MCPL_BCAST_HOST_ACK...\n");
        //sark_delay_us(100000);
        spin1_send_mc_packet(MCPL_BCAST_HOST_ACK, 0, WITH_PAYLOAD);
    }

    // if host send images
	// NOTE: what if we use arg part of SCP for image data? OK let's try, because see HOW.DO...
	else if(port==SDP_PORT_R_IMG_DATA) {
		// io_printf(IO_STD, "R-images\n");
		getImage(msg, SDP_PORT_R_IMG_DATA);
	}
	else if(port==SDP_PORT_G_IMG_DATA) {
		// io_printf(IO_STD, "G-images\n");
        getImage(msg, SDP_PORT_G_IMG_DATA);
	}
	else if(port==SDP_PORT_B_IMG_DATA) {
		// io_printf(IO_STD, "B-images\n");
        getImage(msg, SDP_PORT_B_IMG_DATA);
	}
	spin1_msg_free(msg);
}

void triggerProcessing(uint arg0, uint arg1)
{
	/*
	if(blkInfo->opFilter==1) {
		// broadcast command for filtering
		nFiltJobDone = 0;
		spin1_send_mc_packet(MCPL_BCAST_CMD_FILT, 0, WITH_PAYLOAD);
	}
	else {
		nEdgeJobDone = 0;
		// broadcast command for detection
		spin1_send_mc_packet(MCPL_BCAST_CMD_DETECT, 0, WITH_PAYLOAD);
	}
	*/

	// debugging: see, how many chips are able to collect the images:
	// myDelay();
	// io_printf(IO_STD, "Image is completely received. Ready for processing!\n");
	// return;

	// Let's skip filtering
	nEdgeJobDone = 0;
	nBlockDone = 0;
	spin1_send_mc_packet(MCPL_BCAST_CMD_DETECT, 0, WITH_PAYLOAD);
}

// afterFiltDone() will swap BUFF1_BASE to BUFF0_BASE
void afterFiltDone(uint arg0, uint arg1)
{

}

// check: sendResult() will be executed only by leadAp
// we cannot use workers.imgROut, because workers.imgROut differs from core to core
// use workers.blkImgROut instead!
void sendResult(uint arg0, uint arg1)
{
	// if I'm not include in the list, skip this
	if(blkInfo->maxBlock==0) return;

	//io_printf(IO_STD, "Expecting processing by node-%d\n", arg0);
	if(arg0 != blkInfo->nodeBlockID) return;
	// format sdp (scp_segment + data_segment):
	// cmd_rc = line number
	// seq = sequence of data
	// arg1 = number of data in the data_segment
	// arg2 = channel (R=0, G=1, B=2), NOTE: rgb info isn't in the srce_port!!!
	uchar *imgOut;
	ushort rem, sz;
	uint checkDMA;
	ushort l,c;

	// dtcmImgBuf should be NULL at this point
	/*
	if(dtcmImgBuf != NULL)
		io_printf(IO_BUF, "[Sending] Warning, dtcmImgBuf is not free!\n");
	*/
	dtcmImgBuf == sark_alloc(workers.wImg, sizeof(uchar));
	dLen = SDP_BUF_SIZE + sizeof(cmd_hdr_t);

	hostAck = 1;
	ackCntr = 0;
	resultMsg.srce_port = blkInfo->nodeBlockID + 1;	//+1 to avoid 0 in srce_port

	uint total[3] = {0};
	for(uchar rgb=0; rgb<3; rgb++) {
		if(rgb > 0 && blkInfo->isGrey==1) break;
		resultMsg.arg2 = rgb;

		// io_printf(IO_STD, "[Sending] result channel-%d...\n", rgb);

		l = 0;	// for the imgXOut pointer
		for(ushort lines=workers.blkStart; lines<=workers.blkEnd; lines++) {

			// get the line from sdram
			switch(rgb) {
			case 0: imgOut = workers.blkImgROut + l*workers.wImg; break;
			case 1: imgOut = workers.blkImgGOut + l*workers.wImg; break;
			case 2: imgOut = workers.blkImgBOut + l*workers.wImg; break;
			}

			dmaImgFromSDRAMdone = 0;	// will be altered in hDMA
			do {
				checkDMA = spin1_dma_transfer(DMA_FETCH_IMG_TAG + (myCoreID << 16), (void *)imgOut,
											  (void *)dtcmImgBuf, DMA_READ, workers.wImg);
				if(checkDMA==0)
					io_printf(IO_BUF, "[Sending] DMA full! Retry!\n");

			} while(checkDMA==0);
			// wait until dma is completed
			while(dmaImgFromSDRAMdone==0) {
			}
			// then sequentially copy & send via sdp
			c = 0;
			rem = workers.wImg;
			do {
				//resultMsg.cmd_rc = lines;
				//resultMsg.seq = c+1;
				sz = rem>dLen?dLen:rem;
				//resultMsg.arg1 = sz;
				//resultMsg.srce_port = c+1;
				resultMsg.srce_addr = (lines << 2) + rgb;
				spin1_memcpy((void *)&resultMsg.cmd_rc, (void *)(dtcmImgBuf + c*dLen), sz);
				resultMsg.length = sizeof(sdp_hdr_t) + sz;

				total[rgb] += sz;

				hostAck = 0;
                spin1_send_sdp_msg(&resultMsg, 10);
                // wait for ack if not root-node
                // somehow, root-node doesn't respond to hSDP
                if(sv->p2p_addr != 0) {
					//io_printf(IO_STD, "Waiting ack...\n");
                    while(hostAck==0) {

                    }
					// io_printf(IO_BUF, "Got reply! Continue...\n");
                } else {
					// io_printf(IO_BUF, "[Sending] rgbCh-%d, line-%d, chunk-%d via tag-%d\n", rgb,
					//          lines, c+1, resultMsg.tag);

                    spin1_delay_us(SDP_TX_TIMEOUT);
                    //spin1_delay_us(SDP_TX_TIMEOUT + blkInfo->maxBlock*10);
                }
				c++;		// for the dtcmImgBuf pointer
				rem -= sz;
			} while(rem > 0);
			l++;
		}
	} // end loop channel (rgb)

	//io_printf(IO_STD, "Block-%d done!\n", blkInfo->nodeBlockID);
	//io_printf(IO_BUF, "[Sending] pixels [%d,%d,%d] done!\n", total[0], total[1], total[2]);

	// then send notification to chip<0,0> that my part is complete
	spin1_send_mc_packet(MCPL_BLOCK_DONE, blkInfo->nodeBlockID, WITH_PAYLOAD);

	// release dtcm
	sark_free(dtcmImgBuf);
    dtcmImgBuf = NULL;
}

// afterEdgeDone() send the result to host?
void afterEdgeDone(uint arg0, uint arg1)
{
	// io_printf(IO_STD, "Edge detection done!\n");

	// since each chip holds a part of image data, it needs to send individually to host
    if(blkInfo->nodeBlockID==0)	{   // trigger the chain
        //spin1_schedule_callback(sendResult, 0, 0, PRIORITY_PROCESSING);
        spin1_schedule_callback(sendResult, 0, 0, PRIORITY_LOWEST);


		toc = sv->clock_ms;
		elapse = toc-tic;	// in milliseconds

		// coba pakai timer 2:
		//elapse = READ_TIMER();
    }
		// sendResult(0, 0);
}

void initSDP()
{
	// prepare the reply message
	reportMsg.flags = 0x07;	//no reply
	reportMsg.tag = SDP_TAG_REPLY;
	reportMsg.srce_port = (SDP_PORT_CONFIG << 5) + myCoreID;
	reportMsg.srce_addr = sv->p2p_addr;
	reportMsg.dest_port = PORT_ETH;
	reportMsg.dest_addr = sv->eth_addr;
	reportMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);	// it's fix!

	// prepare the result data
	resultMsg.flags = 0x07;
	resultMsg.tag = SDP_TAG_RESULT;
	// what if:
	// - srce_addr contains image line number + rgb info
	// - srce_port contains the data sequence
	//resultMsg.srce_port = myCoreID;		// during sending, this must be modified
	//resultMsg.srce_addr = sv->p2p_addr;
	resultMsg.dest_port = PORT_ETH;
	resultMsg.dest_addr = sv->eth_addr;
	//resultMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);	// need to be modified later

    // and the debug data
    debugMsg.flags = 0x07;
    debugMsg.tag = SDP_TAG_DEBUG;
    debugMsg.srce_port = (SDP_PORT_CONFIG << 5) + myCoreID;
    debugMsg.srce_addr = sv->p2p_addr;
    debugMsg.dest_port = PORT_ETH;
    debugMsg.dest_addr = sv->eth_addr;
    debugMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);

	// prepare iptag?
	/*
	io_printf(IO_BUF, "reportMsg.tag = %d, resultMsg.tag = %d, debugMsg.tag = %d\n",
			  reportMsg.tag, resultMsg.tag, debugMsg.tag);
	*/
}

/* initRouter() initialize MCPL routing table by leadAp. Use two keys:
 * MCPL_BCAST_KEY and MCPL_TO_LEADER
 * */
void initRouter()
{
	uint allRoute = 0xFFFF80;	// excluding core-0 and external links
	uint leader = (1 << (myCoreID+6));
	uint workers = allRoute & ~leader;
	uint dest;

	// first, set individual destination
    uint e = rtr_alloc(23);
	if(e==0) {
		rt_error(RTE_ABORT);
	} else {
		// each destination core might have its own key association
		// so that leadAp can tell each worker, which region is their part
		for(uint i=0; i<17; i++)
			// starting from core-1 up to core-17
			rtr_mc_set(e+i, i+1, 0xFFFFFFFF, (MC_CORE_ROUTE(i+1)));
	}
	// then add another keys
	e = rtr_alloc(18);
	if(e==0) {
		rt_error(RTE_ABORT);
	} else {
		rtr_mc_set(e, MCPL_BCAST_INFO_KEY, 0xFFFFFFFF, workers); e++;
		rtr_mc_set(e, MCPL_BCAST_CMD_FILT, 0xFFFFFFFF, allRoute); e++;
		rtr_mc_set(e, MCPL_BCAST_CMD_DETECT, 0xFFFFFFFF, allRoute); e++;
		rtr_mc_set(e, MCPL_BCAST_GET_WLOAD, 0xFFFFFFFF, workers); e++;
		rtr_mc_set(e, MCPL_PING_REPLY, 0xFFFFFFFF, leader); e++;
		rtr_mc_set(e, MCPL_FILT_DONE, 0xFFFFFFFF, leader); e++;
		rtr_mc_set(e, MCPL_EDGE_DONE, 0xFFFFFFFF, leader); e++;

		// special for MCPL_BLOCK_DONE
		ushort x = CHIP_X(sv->p2p_addr);
		ushort y = CHIP_Y(sv->p2p_addr);
		if (x>0 && y>0)			dest = (1 << 4);	// south-west
		else if(x>0 && y==0)	dest = (1 << 3);	// west
		else if(x==0 && y>0)	dest = (1 << 5);	// south
		else					dest = leader;
		rtr_mc_set(e, MCPL_BLOCK_DONE, 0xFFFFFFFF, dest); e++;

        // special for MCPL_BCAST_SEND_RESULT, MCPL_BCAST_*pixels and HOST_ACK
		dest = leader;	// by default, go to leadAP, unless:
#ifdef USE_SPIN5
		ushort d;	// distance
		if(x==y) {
			if(x==0)
				dest = 1 + (1 << 1) + (1 << 2);
			else if(x<7)
				dest += 1 + (1 << 1) + (1 << 2);
		}
		else if(x>y) {
			d = x - y;
			if(x<7 && d<4)
				dest += 1;
		}
		else if(x<y) {
			d = y - x;
			if(d<3 && y<7)
				dest += (1 << 2);
		}
#else
		if(sv->p2p_addr==0)
			//dest += 1 + (1<<1) + (1<<2);
			dest = 1 + (1<<1) + (1<<2);
#endif
        // broadcast notification to send result
        rtr_mc_set(e, MCPL_BCAST_SEND_RESULT, 0xFFFFFFFF, dest); e++;
        // broadcasting pixels
		rtr_mc_set(e, MCPL_BCAST_RED_PX, 0xFFFF000F, dest); e++;
		rtr_mc_set(e, MCPL_BCAST_RED_PX_END, 0xFFFF000F, dest); e++;
		rtr_mc_set(e, MCPL_BCAST_GREEN_PX, 0xFFFF000F, dest); e++;
		rtr_mc_set(e, MCPL_BCAST_GREEN_PX_END, 0xFFFF000F, dest); e++;
		rtr_mc_set(e, MCPL_BCAST_BLUE_PX, 0xFFFF000F, dest); e++;
		rtr_mc_set(e, MCPL_BCAST_BLUE_PX_END, 0xFFFF000F, dest); e++;
        rtr_mc_set(e, MCPL_BCAST_IMG_READY, 0xFFFFFFFF, dest); e++;
        rtr_mc_set(e, MCPL_BCAST_HOST_ACK, 0xFFFFFFFF, dest); e++;
		// broadcasting image information
		rtr_mc_set(e, MCPL_BCAST_IMG_INFO_BASE, 0xFFFF0000, dest); e++;
    }
}

void initImage()
{
	blkInfo->imageInfoRetrieved = 0;
	blkInfo->fullRImageRetrieved = 0;
	blkInfo->fullGImageRetrieved = 0;
	blkInfo->fullBImageRetrieved = 0;
	blkInfo->imgRIn = (uchar *)IMG_R_BUFF0_BASE;
	blkInfo->imgGIn = (uchar *)IMG_G_BUFF0_BASE;
	blkInfo->imgBIn = (uchar *)IMG_B_BUFF0_BASE;
	blkInfo->imgROut = (uchar *)IMG_R_BUFF1_BASE;
	blkInfo->imgGOut = (uchar *)IMG_G_BUFF1_BASE;
	blkInfo->imgBOut = (uchar *)IMG_B_BUFF1_BASE;
	dtcmImgBuf = NULL;
    pixelCntr = 0;  // for forwarding using MCPL
}


void initNode(uint arg0, uint arg1)
{
    // adjust, which node am I
    if(sv->p2p_addr != 0) {
        for(ushort i=0; i<nodeCntr; i++) {
            if((chips[i].x == CHIP_X(sv->p2p_addr)) && (chips[i].y == CHIP_Y(sv->p2p_addr))) {
                blkInfo->nodeBlockID = chips[i].id;
                break;
            }
        }
    }
	blkInfo->imageInfoRetrieved = 1;
	initImage();
	spin1_send_mc_packet(MCPL_BCAST_GET_WLOAD, needSendDebug, WITH_PAYLOAD);
	// just for debugging:
	spin1_schedule_callback(printImgInfo, 0, 0, PRIORITY_PROCESSING);
	spin1_schedule_callback(computeWLoad, needSendDebug, 0, PRIORITY_PROCESSING);
}

// terminate gracefully
void cleanUp()
{
	//sark_xfree(sv->sysram_heap, blkInfo, ALLOC_LOCK);
	sark_xfree(sv->sdram_heap, blkInfo, ALLOC_LOCK);
	// in this app, we "fix" the address of image, no need for xfree

	/*
	if(leadAp) {
		sark_free(reportMsg);
		sark_free(resultMsg);
	}
	*/
}

/*_____________________________ Helper/debugging functions _________________________*/

void printImgInfo(uint opType, uint None)
{
	io_printf(IO_BUF, "Image w = %d, h = %d, ", blkInfo->wImg, blkInfo->hImg);
	if(blkInfo->isGrey==1)
		io_printf(IO_BUF, "grascale, ");
	else
		io_printf(IO_BUF, "color, ");
	if(blkInfo->opType==IMG_SOBEL) {
		if(blkInfo->opFilter==IMG_NO_FILTERING)
			io_printf(IO_BUF, "for sobel without filtering\n");
		else
			io_printf(IO_BUF, "for sobel with filtering\n");
	}
	else {
		if(blkInfo->opFilter==IMG_NO_FILTERING)
			io_printf(IO_BUF, "for laplace without filtering\n");
		else
			io_printf(IO_BUF, "for laplace with filtering\n");
	}
	io_printf(IO_BUF, "nodeBlockID = %d with total Block = %d\n",
			  blkInfo->nodeBlockID, blkInfo->maxBlock);
	io_printf(IO_BUF, "Running in mode-%d\n", chainMode);
	if(sv->p2p_addr==0 && chainMode==1) { // print chain
		io_printf(IO_BUF, "Nodes info:\n--------------\n");
		io_printf(IO_BUF, "Node-0 = chip<0,0>\n");
		for(ushort i=0; i<blkInfo->maxBlock-1; i++) {
			io_printf(IO_BUF, "Node-%d = chip<%d,%d>\n", chips[i].id, chips[i].x, chips[i].y);
		}
		io_printf(IO_BUF, "----------------\n");
	}
}

void printWID(uint None, uint Neno)
{
	io_printf(IO_BUF, "Total workers = %d\n", workers.tAvailable);
	for(uint i=0; i<workers.tAvailable; i++)
		io_printf(IO_BUF, "wID-%d is core-%d\n", i, workers.wID[i]);
}

void myDelay()
{
	ushort x = CHIP_X(sv->board_addr);
	ushort y = CHIP_Y(sv->board_addr);
	spin1_delay_us((x*2+y)*1000);
}

/*____________________________________________________ Helper/debugging functions __*/
