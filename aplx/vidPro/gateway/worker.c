
#include "SpiNNEdge.h"
#include <stdlib.h>	// need for abs()

void printWLoad()
{
    io_printf(IO_BUF, "debugMsg.tag = %d\n", debugMsg.tag);
	// io_printf(IO_BUF, "wID = %d\n", workers.wID[myCoreID]);
	io_printf(IO_BUF, "subBlockID = %d\n", workers.subBlockID);
	io_printf(IO_BUF, "tAvailable = %d\n", workers.tAvailable);
	io_printf(IO_BUF, "blkStart = %d\n", workers.blkStart);
	io_printf(IO_BUF, "blkEnd = %d\n", workers.blkEnd);
	io_printf(IO_BUF, "nLinesPerBlock = %d\n", workers.nLinesPerBlock);
	io_printf(IO_BUF, "startLine = %d\n", workers.startLine);
	io_printf(IO_BUF, "endLine = %d\n", workers.endLine);
	io_printf(IO_BUF, "wImg = %d\n", workers.wImg);
	io_printf(IO_BUF, "hImg = %d\n", workers.hImg);
	io_printf(IO_BUF, "imgRIn @ 0x%x\n", workers.imgRIn);
	io_printf(IO_BUF, "imgGIn @ 0x%x\n", workers.imgGIn);
	io_printf(IO_BUF, "imgBIn @ 0x%x\n", workers.imgBIn);
	io_printf(IO_BUF, "imgROut @ 0x%x\n", workers.imgROut);
	io_printf(IO_BUF, "imgGOut @ 0x%x\n", workers.imgGOut);
	io_printf(IO_BUF, "imgBOut @ 0x%x\n", workers.imgBOut);
}

// compute individual working block
void computeWLoad(uint withReport, uint arg1)
{
	ushort w = blkInfo->wImg; workers.wImg = w;
	ushort h = blkInfo->hImg; workers.hImg = h;

	// block-wide
	workers.nLinesPerBlock = h / blkInfo->maxBlock;
	ushort nRemInBlock = h % blkInfo->maxBlock;
	workers.blkStart = blkInfo->nodeBlockID * workers.nLinesPerBlock;

	// core-wide
	if(blkInfo->nodeBlockID==blkInfo->maxBlock-1)
		workers.nLinesPerBlock += nRemInBlock;
	workers.blkEnd = workers.blkStart + workers.nLinesPerBlock - 1;

	ushort nLinesPerCore = workers.nLinesPerBlock / workers.tAvailable;
	ushort nRemInCore = workers.nLinesPerBlock % workers.tAvailable;
	ushort wl[17], sp[17], ep[17];	// assuming 17 cores at max
	ushort i,j;

	// get the basic information
	/*
	io_printf(IO_BUF, "myWID = %d, tAvailable = %d\n", workers.subBlockID, workers.tAvailable);
	io_printf(IO_BUF, "wImg = %d, hImg = %d\n", blkInfo->wImg, blkInfo->hImg);
	io_printf(IO_BUF, "nodeBlockID = %d, maxBlock = %d\n", blkInfo->nodeBlockID, blkInfo->maxBlock);
	io_printf(IO_BUF, "nLinesPerBlock = %d\n", nLinesPerBlock);
	io_printf(IO_BUF, "blkStart = %d\n", blkStart);
	*/

	// initialize starting point with respect to blkStart
	for(i=0; i<17; i++)
		sp[i] = workers.blkStart;

	for(i=0; i<workers.tAvailable; i++) {
		wl[i] = nLinesPerCore;
		if(nRemInCore > 0) {
			wl[i]++;
			for(j=i+1; j<workers.tAvailable; j++) {
				sp[j]++;
			}
			nRemInCore--;
		}
		sp[i] += i*nLinesPerCore;
		ep[i] = sp[i]+wl[i]-1;
	}
	workers.startLine = sp[workers.subBlockID];
	workers.endLine = ep[workers.subBlockID];

	// then align the internal/worker pointer accordingly
	workers.imgRIn = blkInfo->imgRIn + w*workers.startLine;
	workers.imgGIn = blkInfo->imgGIn + w*workers.startLine;
	workers.imgBIn = blkInfo->imgBIn + w*workers.startLine;
	workers.imgROut = blkInfo->imgROut + w*workers.startLine;
	workers.imgGOut = blkInfo->imgGOut + w*workers.startLine;
	workers.imgBOut = blkInfo->imgBOut + w*workers.startLine;
	// so, each work has different value of those workers.img*

	// leadAp needs to know, the address of image block
	// it will be used for sending result to host-PC
	if(leadAp) {
		uint szBlk = (workers.blkEnd - workers.blkStart + 1) * workers.wImg;
		uint offset = blkInfo->nodeBlockID * szBlk;
		workers.blkImgRIn = blkInfo->imgRIn + offset;
		workers.blkImgGIn = blkInfo->imgGIn + offset;
		workers.blkImgBIn = blkInfo->imgBIn + offset;
		workers.blkImgROut = blkInfo->imgROut + offset;
		workers.blkImgGOut = blkInfo->imgGOut + offset;
		workers.blkImgBOut = blkInfo->imgBOut + offset;
	}

	if(withReport==1) {
		// let's print the resulting workload
		// io_printf(IO_BUF, "sp = %d, ep = %d\n", workers.startLine, workers.endLine);
		// printWLoad();
		debugMsg.cmd_rc = blkInfo->nodeBlockID;
		debugMsg.seq = workers.subBlockID;
		debugMsg.arg1 = (workers.startLine << 16) + workers.endLine;
		debugMsg.arg2 = (uint)workers.imgRIn;
		debugMsg.arg3 = (uint)workers.imgROut;
		spin1_delay_us((blkInfo->nodeBlockID*17+workers.subBlockID)*100);

		uint checkSDP = 0;
		do {
			checkSDP = spin1_send_sdp_msg(&debugMsg, 10);
			if(checkSDP==0)
				io_printf(IO_BUF, "Send SDP fail! Retry!");
		} while(checkSDP == 0);

		io_printf(IO_BUF, "debugMsg is sent via tag-%d!\n", debugMsg.tag);
	}
}

void imgFiltering(uint arg0, uint arg1)
{

}

void imgDetection(uint arg0, uint arg1)
{
	//io_printf(IO_BUF, "Begin edge detection...\n");
	ushort szMask = blkInfo->opType == IMG_SOBEL ? 3:5;
	ushort offset = blkInfo->opType == IMG_SOBEL ? 1:2;
	ushort w = blkInfo->wImg;
	ushort h = blkInfo->hImg;
	uint cntPixel = szMask * w;
	short l,c,n,i,j;
	uchar *sdramImgIn, *sdramImgOut;
	uchar *dtcmLine;
	int sumX, sumY, sumXY;

	uchar *resImgBuf;

	uchar rgbCntr;
	uint dmaCheck;

	// how many lines this worker has?
	n = workers.endLine - workers.startLine + 1;
	// when first called, dtcmImgBuf should be NULL
	// and img*In must point to the BASE
	dtcmImgBuf = sark_alloc(cntPixel, sizeof(uchar));
	resImgBuf = sark_alloc(w, sizeof(uchar));	// just one line!

	// for all color channels
	for(rgbCntr=0; rgbCntr<3; rgbCntr++) {

		// scan for all lines in the working block
		for(l=0; l<n; l++) {
			// get the current line address in sdram
			switch(rgbCntr) {
			case 0:
				sdramImgIn	= workers.imgRIn + l*w;
				sdramImgOut = workers.imgROut + l*w; break;
			case 1:
				sdramImgIn	= workers.imgGIn + l*w;
				sdramImgOut = workers.imgGOut + l*w; break;
			case 2:
				sdramImgIn	= workers.imgBIn + l*w;
				sdramImgOut = workers.imgBOut + l*w; break;
			}

			// shift by mask size for fetching via dma
			sdramImgIn -= offset*w;

			dmaImgFromSDRAMdone = 0;
			do {
				dmaCheck = spin1_dma_transfer((myCoreID << 16) +  DMA_FETCH_IMG_TAG, (void *)sdramImgIn,
								   (void *)dtcmImgBuf, DMA_READ, cntPixel);
				/*
				if(dmaCheck==0)
					io_printf(IO_BUF, "[Edging] DMA full! Retry...\n");
				else
					io_printf(IO_BUF, "[Edging] Got DMA!\n");
				*/
			} while (dmaCheck==0);
			//if(dmaCheck==0) {
			//	io_printf(IO_BUF, "DMA full!\n");
			//}
			while(dmaImgFromSDRAMdone==0) {
			}

			// point to the current image line in the DTCM (not in SDRAM!)
			dtcmLine = dtcmImgBuf + offset*w;	// mind the offset since dtcmImgBuf contains additional data

			// scan for all column in the line
			for(c=0; c<w; c++) {
				// if offset is 1, then it is for sobel, otherwise it is for laplace
				if(offset==1) {
					sumX = 0;
					sumY = 0;
					if(workers.startLine+l == 0 || workers.startLine+l == h-1)
						sumXY = 0;
					else if(c==0 || c==w-1)
						sumXY = 0;
					else {
						for(i=-1; i<=1; i++)
							for(j=-1; j<=1; j++) {
								sumX += (int)((*(dtcmLine + c + i + j*w)) * GX[i+1][j+1]);
								sumY += (int)((*(dtcmLine + c + i + j*w)) * GY[i+1][j+1]);
							}
						// python version: sumXY[0] = math.sqrt(math.pow(sumX[0],2) + math.pow(sumY[0],2))
						sumXY = (abs(sumX) + abs(sumY))*7/10;	// 7/10 = 0.717 -> cukup dekat dengan akar
					}
				}
				else {	// for laplace operation
					sumXY = 0;
					if((workers.startLine+l) < 2 || (h-workers.startLine+l) <= 2)
						sumXY = 0;
					else if(c<2 || (w-c)<=2)
						sumXY = 0;
					else {
						for(i=-1; i<=2; i++)
							for(j=-2; j<=2; j++)
								sumXY += (int)((*(dtcmLine + c + i + j*w)) * LAP[i+2][j+2]);
					}
				}
				if(sumXY>255) sumXY = 255;
				if(sumXY<0) sumXY = 0;
				// resImgBuf is just one line and it doesn't matter, where it is!
				*(resImgBuf + c) = 255 - (uchar)(sumXY);
				//*(resImgBuf + c) = (uchar)(sumXY);
				// TODO: what if dma full?
				spin1_dma_transfer((myCoreID << 16) + DMA_STORE_IMG_TAG, (void *)sdramImgOut,
								   (void *)resImgBuf, DMA_WRITE, w);
			} // end for c-loop
		} // end for l-loop

		// if img is grey, stop with R-channel only
		if(rgbCntr>=1 && blkInfo->isGrey==1)
			break;
	} // end of for all color channels

	// clean-up memory in DTCM
    sark_free(resImgBuf);  //resImgBuf = NULL;
    sark_free(dtcmImgBuf); //dtcmImgBuf = NULL; // -> this will create WDOG!!!
	// at the end, send MCPL_EDGE_DONE
	// io_printf(IO_BUF, "Done! send MCPL_EDGE_DONE!\n");
	spin1_send_mc_packet(MCPL_EDGE_DONE, 0, WITH_PAYLOAD);
}
