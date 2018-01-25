#include "gateway.h"

uint initApp()
{
    uint ok;
    wInfo.coreID = sark_core_id();

    // sanity check first
    if(sv->p2p_addr!=GATEWAY_CHIP || wInfo.coreID<2) {
        io_printf(IO_STD, "[ERR] Wrong core/chip!\n");
        ok = FAILURE;
    }
    else if(sark_app_id()!=GATEWAY_APP_ID) {
        io_printf(IO_STD, "[ERR] Wrong app-ID!\n");
        ok = FAILURE;
    }
    else {
        imgBufInitialized = FALSE;
        imgBufIn = NULL;
        imgBufOut = NULL;

        // initialize local buffer for pixel chunk streaming
        pxBuf.RGBbuf  = sark_alloc(MAX_PIXEL_IN_CHUNK, 3);   //264-bytes
        pxBuf.Graybuf = sark_alloc(MAX_PIXEL_IN_CHUNK, 1);  //88-bytes
        histogram = sark_alloc(256, sizeof(ushort));

        pxBuf.ptrRGBbuf  = pxBuf.RGBbuf;
        pxBuf.ptrGraybuf = pxBuf.Graybuf;

        if(pxBuf.RGBbuf==NULL || pxBuf.Graybuf==NULL || histogram==NULL) {
            ok = FAILURE;
        }
        else {
            // continue with worker active identification and buffer initialization
            if(wInfo.coreID>=BCAST_IMG_STARTING_CORE && wInfo.coreID<=BCAST_IMG_ENDING_CORE)
                wInfo.active=TRUE;
            else
                wInfo.active=FALSE;
            wInfo.dtcmImgBuf=NULL;
            ok = SUCCESS;
        }
    }

#if(DEBUG_LEVEL>0)
    if(ok==SUCCESS){
        if(wInfo.coreID==LEAD_CORE)
            io_printf(IO_STD, "[INFO] gateway is loaded!\n");
        else
            io_printf(IO_BUF, "[INFO] gateway is loaded!\n");
    }
#endif
    return ok;
}

/* initRouter() should be done only by leadAp
 * */
uint initRouter()
{
    uint i,ok,key,mask,route,nEntry;

    mask = MCPL_FWD_PIXEL_MASK;

    // allocate entry for:
    // - 30 for fwding pixels SOC and DATA
    // - 1  for fwding EOF
    // - 29 for histogram chain
    // - BCAST_IMG_NUM_CORE for bcasting image
    // - BCAST_IMG_NUM_CORE for bcasting image info
    // - BCAST_IMG_NUM_CORE for acknowledge of bcasting image
    // - 1  for bcasting histogram
    // - 1  for MCPL_FWD_IMAGE_INFO
    nEntry = N_CORE_FOR_FWD*2 + 1 + 29 + (3*BCAST_IMG_NUM_CORE) + 1 + 1;

    uint e = rtr_alloc(nEntry);
    if(e==0) {
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[ERR] initRouter fail\n");
#endif
        ok = FAILURE;
    }
    else {

        // set fwding SOC and DATA
        for(i=0; i<N_CORE_FOR_FWD; i++) {
            route = 1 << (LEAD_CORE+6+i);
            key = MCPL_FWD_PIXEL_SOC | (LEAD_CORE+i);
            rtr_mc_set(e++,key,mask,route);
            key = MCPL_FWD_PIXEL_DATA | (LEAD_CORE+i);
            rtr_mc_set(e++,key,mask,route);
        }
        // set fwding EOF
        route = 1 << (LEAD_CORE+6);
        key = MCPL_FWD_PIXEL_EOF | LEAD_CORE;
        rtr_mc_set(e++, key, mask, route);
        ok = SUCCESS;


        // set histogram chain mechanism
        // for LEAD_CORE
        route = N_CORE_FOR_HIST_BITS;
        key = MCPL_HIST_CHAIN_NEXT | LEAD_CORE;
        rtr_mc_set(e++,key,mask,route);
        for(i=0; i<3; i++){
            //for core 3,4,5
            route = 1 << (6+i+4);
            key = MCPL_HIST_CHAIN | (i+3);       rtr_mc_set(e++,key,mask,route);
            key = MCPL_HIST_CHAIN_NEXT | (i+3);  rtr_mc_set(e++,key,mask,route);
            //for core 9,8,7
            route = 1 << (6+8-i);
            key = MCPL_HIST_CHAIN | (9-i);       rtr_mc_set(e++,key,mask,route);
            key = MCPL_HIST_CHAIN_NEXT | (9-i);  rtr_mc_set(e++,key,mask,route);
            //for core 10,11,12
            route = 1 << (6+i+11);
            key = MCPL_HIST_CHAIN | (i+10);      rtr_mc_set(e++,key,mask,route);
            key = MCPL_HIST_CHAIN_NEXT | (i+10); rtr_mc_set(e++,key,mask,route);
            //for core 16,15,14
            route = 1 << (6+15-i);
            key = MCPL_HIST_CHAIN | (16-i);      rtr_mc_set(e++,key,mask,route);
            key = MCPL_HIST_CHAIN_NEXT | (16-i); rtr_mc_set(e++,key,mask,route);
        }
        //for core 6 and 13
        route = 1 << (6+LEAD_CORE);
        key = MCPL_HIST_CHAIN | 6;               rtr_mc_set(e++,key,mask,route);
        key = MCPL_HIST_CHAIN_NEXT | 6;          rtr_mc_set(e++,key,mask,route);
        key = MCPL_HIST_CHAIN | 13;              rtr_mc_set(e++,key,mask,route);
        key = MCPL_HIST_CHAIN_NEXT | 13;         rtr_mc_set(e++,key,mask,route);

        /* set for broadcasting image and its info
         * format:
         *   key = 0xbca1 coreID
         *   payload = 4 pixels or current line number
         * */
        mask  = MCPL_BCAST_IMG_MASK;
        route = 1 << NEAST_LINK; //go to chip <3,3>
        for(i=0;i<BCAST_IMG_NUM_CORE;i++) {
            key = MCPL_BCAST_IMG | (BCAST_IMG_STARTING_CORE+i);
            rtr_mc_set(e++,key,mask,route);
            key = MCPL_BCAST_IMG_INFO | (BCAST_IMG_STARTING_CORE+i);
            rtr_mc_set(e++,key,mask,route);
        }

        /* set for the ackownledge of broadcasting image */
        mask = MCPL_BCAST_IMG_MASK;
        for(i=0; i<BCAST_IMG_NUM_CORE; i++) {
            key = MCPL_BCAST_IMG_ACK | (BCAST_IMG_NUM_CORE+i);
            route = 1 << BCAST_IMG_STARTING_CORE+i+6;
            rtr_mc_set(e++,key,mask,route);
        }

        // set for broadcasting histogram: only by LEAD_CORE
        mask  = MCPL_BCAST_HIST_MASK;
        key   = MCPL_BCAST_HIST;
        route = 1 << NEAST_LINK; //go to chip <3,3>
        rtr_mc_set(e++,key,mask,route);

        // set for MCPL_FWD_IMAGE_INFO (from gateway to vidpro)
        mask = MCPL_FWD_IMG_INFO_MASK; // reserve the low word for additional image info!
        route = 0x2A; // to external links 1,3, and 5 (101010==2A)
        key = MCPL_FWD_IMG_INFO;
        rtr_mc_set(e++,key,mask,route);
    }

    return ok;
}

void initHandlers()
{
    spin1_callback_on(MCPL_PACKET_RECEIVED, hMCPL, PRIORITY_MCPL);
    spin1_callback_on(MC_PACKET_RECEIVED, hMC, PRIORITY_MCPL);
    spin1_callback_on(SDP_PACKET_RX, hSDP, PRIORITY_SDP);
    spin1_callback_on(DMA_TRANSFER_DONE, hDMA, PRIORITY_DMA);
    spin1_callback_on(FRPL_PACKET_RECEIVED, hFRPL, PRIORITY_FRPL);
    spin1_callback_on(FR_PACKET_RECEIVED, hFR, PRIORITY_FRPL);
}

/* initImgBuf() initialize image buffer in sdram.
 * Should be called only by LEAD_CORE with option:
 * - create_or_del 0 -> create, 1 -> free
 * - keep_buf 0 -> create new buffer, 1 -> use existing buffer
 *
 * After creating, tells cores:
 * - where imgBufIn and imgBufOut are located
 * - reset their histogram
 */
void initImgBuf(uint create_or_del, uint keep_buf)
{
    if(create_or_del==0) {
        if(keep_buf==0) {
            // resize image buffer if necessary
            if(imgBufInitialized==TRUE){
                sark_xfree(sv->sdram_heap, imgBufIn, ALLOC_LOCK);
                sark_xfree(sv->sdram_heap, imgBufOut, ALLOC_LOCK);
            }
            imgBufIn = sark_xalloc(sv->sdram_heap, frameInfo.szPixmap, 0, ALLOC_LOCK);
            imgBufOut = sark_xalloc(sv->sdram_heap, frameInfo.szPixmap, 0, ALLOC_LOCK);
            if(imgBufIn==NULL || imgBufOut==NULL) {
                rt_error(RTE_MALLOC);
#if(DEBUG_LEVEL>0)
                io_printf(IO_STD, "[ERR] Cannot allocate image buffers!\n");
#endif
            }
            else {
                imgBufInitialized=TRUE;
                // Then tell its cores to be ready for receiving the streamed pixels
                // --> use FR to inform cores
                bcastFR(FRPL_IMGBUFIN_ADDR, (uint)imgBufIn, WITH_PAYLOAD);
                bcastFR(FRPL_IMGBUFOUT_ADDR, (uint)imgBufOut, WITH_PAYLOAD);

                //spin1_send_fr_packet(FRPL_FWD_PIXEL_SOC, 0, NO_PAYLOAD);

            }
        }
        // either keep_buf or not, tell cores to reset histogram
        bcastFR(FRPL_RESET_HISTOGRAM, 0, NO_PAYLOAD);
    }
    else {
        sark_xfree(sv->sdram_heap, imgBufIn, ALLOC_LOCK);
        sark_xfree(sv->sdram_heap, imgBufOut, ALLOC_LOCK);
        imgBufInitialized=FALSE;
    }
}

