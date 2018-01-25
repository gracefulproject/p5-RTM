#include "chipFwdr.h"

// iptag
// IPTAG definitions from scamp:
#define IPTAG_NEW		0
#define IPTAG_SET		1
#define IPTAG_GET		2
#define IPTAG_CLR		3
#define IPTAG_TTO		4

sdp_msg_t iptag;
/* initIPtag() should be executed by leadAp. There are three phases:
 * phase-0: will be called by c_main to init iptag and schedule it
 * phase-1: will send IPTAG_GET to collect host IP address
 * phase-2: will set iptag
 * */
void initIPtag(uint phase, uint hostIP)
{
    if(phase==0) {
        // prepare sdp container for iptag
        iptag.flags = 0x87;	// initially needed for iptag triggering
        iptag.tag = 0;		// internal
        iptag.srce_addr = sv->p2p_addr;
        iptag.srce_port = (SDP_INTERNAL_PORT << 5) + (uchar)coreID;	// use port-7
        iptag.dest_addr = sv->p2p_addr;
        iptag.dest_port = 0;				// send to "root" (scamp)
        iptag.cmd_rc = CMD_IPTAG;
        iptag.arg1 = (IPTAG_GET << 16) + 0; // target iptag-0
        iptag.arg2 = 1; //remember, scamp use this for some reason
        iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
        spin1_schedule_callback(initIPtag, 1, 0, PRIORITY_LOW);
    }
    else if(phase==1) {
        spin1_send_sdp_msg(&iptag, 10);
    }
    else if(phase==2) {
        iptag.flags = 7;
        iptag.arg1 = (IPTAG_SET << 16) + SDP_RESULT_IPTAG;
        iptag.arg2 = SDP_RESULT_PORT;
        iptag.arg3 = hostIP;
        spin1_send_sdp_msg(&iptag, 10); // for sending result data
        iptag.arg1 = (IPTAG_SET << 16) + SDP_GENERIC_IPTAG;
        iptag.arg2 = SDP_GENERIC_PORT;
        spin1_send_sdp_msg(&iptag, 10); // for sending generic info
    }
}

uint initApp()
{
    uint ok;
    coreID = sark_core_id();
    if(sv->p2p_addr!=0 || coreID<2) {
        io_printf(IO_STD, "[ERR] Wrong core/chip!\n");
        ok = FAILURE;
    }
    else if(sark_app_id()!=CHIPFWDR_APP_ID) {
        io_printf(IO_STD, "[ERR] Wrong app-ID!\n");
        ok = FAILURE;
    }
    else {
#if(DEBUG_LEVEL>0)
        if(coreID==LEAD_CORE)
            io_printf(IO_STD, "[INFO] chipFwdr is loaded!\n");
        else
            io_printf(IO_BUF, "[INFO] chipFwdr is loaded!\n");
#endif
        ok = SUCCESS;
    }

    return ok;
}

/* initRouter() should be done only by leadAp
 * */
uint initRouter()
{
    uint i,ok,key,mask,route,nEntry;

    mask = MCPL_FWD_PIXEL_MASK;

    // allocate entry for:
    // - 10 for fwding pixels SOC and DATA
    // - 1  for fwding EOF
    nEntry = N_CORE_FOR_FWD*2 + 1;

    uint e = rtr_alloc(nEntry);
    if(e==0) {
#if(DEBUG_LEVEL>0)
        io_printf(IO_STD, "[ERR] initRouter fail\n");
#endif
        ok = FAILURE;
    }
    else {
        route = 1 << 1;
        // set fwding SOC and DATA
        for(i=0; i<N_CORE_FOR_FWD; i++) {
            key = MCPL_FWD_PIXEL_SOC | (LEAD_CORE+i);
            rtr_mc_set(e++,key,mask,route);
            key = MCPL_FWD_PIXEL_DATA | (LEAD_CORE+i);
            rtr_mc_set(e++,key,mask,route);
        }
        // set fwding EOF
        key = MCPL_FWD_PIXEL_EOF | coreID;
        rtr_mc_set(e++, key, mask, route);
        ok = SUCCESS;
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
}
