#include <spin1_api.h>
#include "SpiNNEdge.h"

void hDMADone(uint tid, uint tag)
{

	if(tag == DMA_STORE_IMG_TAG) {
		dmaImg2SDRAMdone = 1;
		switch(whichRGB) {
		case SDP_PORT_R_IMG_DATA: blkInfo->imgRIn += dLen; break;
		case SDP_PORT_G_IMG_DATA: blkInfo->imgGIn += dLen; break;
		case SDP_PORT_B_IMG_DATA: blkInfo->imgBIn += dLen; break;
		}
        //io_printf(IO_BUF, "dma dLen = %d\n", dLen);
	}
	else if((tag & 0xFFFF) == DMA_FETCH_IMG_TAG) {
		//io_printf(IO_BUF, "dma tag = 0x%x\n", tag);
		if((tag >> 16) == myCoreID) {
			dmaImgFromSDRAMdone = 1;	// sor the image processing can continue
			//io_printf(IO_BUF, "for me!\n");
		}
	}
}

// only core <0,0,leadAp> will do
void notifyHostDone(uint arg0, uint arg1)
{
	//io_printf(IO_STD, "Processing done at %u => total = %u-microsec!\n", toc, total);
	//io_printf(IO_STD, "Processing done in %d-msec!\n", elapse);
	//io_printf(IO_STD, "Processing done\n");
	resultMsg.srce_addr = elapse;
	resultMsg.length = sizeof(sdp_hdr_t);
	spin1_send_sdp_msg(&resultMsg, 10);
}

void hMCPL(uint key, uint payload)
{
	// NOTE: DON'T DO io_printf() here!!!!!
	// io_printf(IO_BUF, "Got key-0x%x, payload-0x%x\n", key, payload);
    //------------------------ this is worker part --------------------------
    if(key==MCPL_BCAST_INFO_KEY) {
		// leadAp sends "0" for ping, worker replys with its core
		if(payload==0)
			spin1_send_mc_packet(MCPL_PING_REPLY, myCoreID, WITH_PAYLOAD);
		else
			blkInfo = (block_info_t *)payload;
	}
	else if(key==myCoreID) {
		workers.tAvailable = payload >> 16;
		workers.subBlockID = payload & 0xFFFF;
	}
	else if(key==MCPL_BCAST_CMD_FILT) {
		spin1_schedule_callback(imgFiltering, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BCAST_CMD_DETECT) {
		spin1_schedule_callback(imgDetection, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BCAST_GET_WLOAD) {
		spin1_schedule_callback(computeWLoad, 0, 0, PRIORITY_PROCESSING);
	}
	//------------------------ this is leadAp only part --------------------------
	else if(key==MCPL_BCAST_SEND_RESULT) {
		//if(payload == blkInfo->nodeBlockID) {
            spin1_schedule_callback(sendResult, payload, 0, PRIORITY_LOWEST);
			// io_printf(IO_BUF, "pay=%d!\n", payload);
			//hostAck = 1;
			//ackCntr++;
		//}
	}
	else if(key==MCPL_PING_REPLY) {
		workers.wID[workers.tAvailable] = payload;	// this will be automatically mapped to workers.subBlockID[workers.tAvailable]
		workers.tAvailable++;
	}
	else if(key==MCPL_FILT_DONE) {
		nFiltJobDone++;
		if(nFiltJobDone==workers.tAvailable)
			spin1_schedule_callback(afterFiltDone, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_EDGE_DONE) {
		nEdgeJobDone++;
		if(nEdgeJobDone==workers.tAvailable)
			spin1_schedule_callback(afterEdgeDone, 0, 0, PRIORITY_PROCESSING);
	}
	else if(key==MCPL_BLOCK_DONE) {
		nBlockDone++;
		if(nBlockDone==blkInfo->maxBlock) {
			spin1_schedule_callback(notifyHostDone, 0, 0, PRIORITY_PROCESSING);
		}
		// if I'm block-0, continue the chain
		//else {
		else if(blkInfo->nodeBlockID==0) {
			spin1_send_mc_packet(MCPL_BCAST_SEND_RESULT, ++payload, WITH_PAYLOAD);
		}
	}
    // MCPL_BCAST_IMG_READY should be put prior to the other pixel-related keys
    else if(key == MCPL_BCAST_IMG_READY) {
        spin1_schedule_callback(afterCollectPixel, payload, 0, PRIORITY_PROCESSING);
    }
    else if(key==MCPL_BCAST_HOST_ACK) {
        //io_printf(IO_STD, "MCPL_BCAST_HOST_ACK is received. Setting hostAck to 1.\n");
        hostAck = 1;
    }
	//else if((key >> 4) == 0x0bca5fff) {
	else if((key & 0xFFFF0000) == MCPL_BCAST_PIXEL_BASE) {	// because pixel index is carried on in the key
		if((key & 1) == 0) {
			spin1_schedule_callback(collectPixel, key, payload, PRIORITY_PROCESSING);
		}
		else {
			//io_printf(IO_BUF, "Got pixel-%d: 0x%x\n", (key >> 4) & 0xfff, payload);
			// we put the index in the key as well
			pixelCntr = (key >> 4) & 0xfff;
			//sark_mem_cpy((void *)&pixelBuffer[pixelCntr], (void *)&payload, sizeof(uint));
			pixelBuffer[pixelCntr] = payload;
			//pixelCntr++;
			// check if "END-transmission" is sent
		}
    }
	else if((key & 0xFFFF0000) == MCPL_BCAST_IMG_INFO_BASE) {
        uint keyType = key & 0xF;
        //io_printf(IO_BUF, "Got keyType-%d\n", keyType);
        if(keyType == 0) {
			nodeCntr = 0;
		}
		// got node info?
        else if(keyType == 2) {
			// NOTE: the root (node-0) is chip<0,0>
			chips[nodeCntr].id = (key >> 4) & 0xfff;
			chips[nodeCntr].x = payload >> 16;
			chips[nodeCntr].y = payload & 0xFFFF;
			nodeCntr++;
            /*
            io_printf(IO_BUF, "Node-%d = <%d,%d>\n", chips[nodeCntr-1].id,
                    chips[nodeCntr-1].x, chips[nodeCntr-1].y);
            */
		}
		// got image size and max block?
        else if(keyType == 1) {
			blkInfo->wImg = payload >> 16;
			blkInfo->hImg = payload & 0xFFFF;
			blkInfo->maxBlock = (key >> 4) & 0xfff;
		}
		// got operation type?
        else if(keyType == 3) {
			needSendDebug = (key >> 4) & 0xfff;            
			blkInfo->isGrey = payload >> 8;
			switch(payload & 0xFF) {
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
            /*
            io_printf(IO_BUF, "Got seq = 0x%x\n", payload);
            sark_delay_us(1000000);
            */
		}
		// got EOF?
		else {
			//io_printf(IO_BUF, "Got EOF key-0x%x, payload-0x%x\n", key, payload);
			spin1_schedule_callback(initNode, 0, 0, PRIORITY_PROCESSING);
		}
	}
	else {
		io_printf(IO_BUF, "Got key-0x%x, payload-0x%x\n", key, payload);
	}
}

void c_main()
{
	myCoreID = sark_core_id();
	spin1_callback_on(MCPL_PACKET_RECEIVED, hMCPL, PRIORITY_MCPL);
	spin1_callback_on(DMA_TRANSFER_DONE, hDMADone, PRIORITY_DMA);
	initSDP();	// sebelumnya hanya leadAp, tapi masalahnya semua core butuh reportMsg dan debugMsg

	// enable timer-2
	ENABLE_TIMER ();	// Enable timer (once)


	// only leadAp has access to dma and sdp
	if(leadAp) {

		io_printf(IO_BUF, "My p2paddr = %d\n", sv->p2p_addr);
		// the danger of sdp_msg_t * without initialization is...
		/*
		reportMsg = sark_alloc(1, sizeof(sdp_msg_t));
		resultMsg = sark_alloc(1, sizeof(sdp_msg_t));
		io_printf(IO_BUF, "reportMsg @ 0x%x, resultMsg @ 0x%x\n", reportMsg, resultMsg);
		*/

		// prepare chip-level image block information
		//blkInfo = sark_xalloc(sv->sysram_heap, sizeof(block_info_t),
		//                      sark_app_id(), ALLOC_LOCK);
		blkInfo = sark_xalloc(sv->sdram_heap, sizeof(block_info_t),
									  sark_app_id(), ALLOC_LOCK);
		if(blkInfo==NULL) {
			io_printf(IO_BUF, "blkInfo alloc error!\n");
			rt_error(RTE_ABORT);
		}
		else {
			// let's setup basic block info to block non-node chips:
			blkInfo->maxBlock = 0;

			initImage();	// some of blkInfo are initialized there
		}
		spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
		spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
		spin1_callback_on(SDP_PACKET_RX, hSDP, PRIORITY_SDP);

		workers.wID[0] = myCoreID;
		workers.tAvailable = 1;
		workers.subBlockID = 0;	// leadAp has task ID-0
		//chips = NULL;
		nodeCntr = 0;
		initRouter();
		initIPTag();
	}
	else {
		io_printf(IO_BUF, "SpiNNEdge running @ core-%d id-%d\n",
				  sark_core_id(), sark_app_id());
	}

	// let's test blkInfo

	spin1_start(SYNC_NOWAIT);
}
