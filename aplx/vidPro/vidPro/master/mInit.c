#include "mVidPro.h"

/* checkLinks() check if SpiNN-link is active
 * Normally, we use ybug lmemw to check if a link is active.
 *
 * Here, we use another trick, even though not 100% correct
 * The logic:
 * - cek link to east:
 *   if p2p_addr to the chip to the east doesn't exist, the link is dead
 *   but if the p2p_addr exists, then we ASSUME that the link is actually exist
 * - etc. for other direction
 * NOTE: this might be invalid, for example if p2p is actually re-route because
 *       the link is dead. In this program, we ASSUME this never happens :)
 * */
uchar linkState[6] = {0};
static void checkLinks()
{
    uint i;
    ushort x=CHIP_X(sv->p2p_addr);
    ushort y=CHIP_Y(sv->p2p_addr);

    // check east link
    if(rtr_p2p_get(((x+1)<<8)|y)!=6) linkState|=1;

    // check north east link
    if(rtr_p2p_get((((x+1)<<8))|(y+1))!=6) linkState|=0x10;

    // check north link
    if(rtr_p2p_get((x<<8)|(y+1))!=6) linkState|=0x100;

    // check west link
    if(rtr_p2p_get(((x-1)<<8)|y)!=6) linkState|=0x1000;

    // check south west link
    if(rtr_p2p_get(((x-1)<<8)|(y-1))!=6) linkState|=0x10000;

    // check south link
    if(rtr_p2p_get((x<<8)|(y-1))!=6) linkState|=0x100000;
}

uint initApp()
{
    uint ok;
    wInfo.coreID = sark_core_id();
    if(wInfo.coreID != LEAD_CORE) {
        io_printf(IO_STD, "[ERR] Wrong core for master!\n");
        ok = FAILURE;
    }
    else if(sark_app_id()!=M_VIDPRO_NODE_APP_ID) {
        io_printf(IO_STD, "[ERR] Wrong app-id!\n");
        ok = FAILURE;
    }
    else {
        initHandlers();
        checkLinks();
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
    spin1_callback_on(FR_PACKET_RECEIVED, hFR, PRIORITY_FRPL);
}

uint initRouter()
{
    {
        uint ok, nEntry, e, i;
        uint key, mask, route;
        // entry:
        // - NUM_CORE_WORKER
        // - 1 for MCPL_FWD_IMG_INFO, sent by gateway
        nEntry = N_CORE_WORKER + 1;
        e = rtr_alloc(nEntry);
        if(e==0) {
            ok = FAILURE;
            io_printf(IO_STD, "[ERR] rtr_alloc\n");
        }
        else {
            // setup for local (intrachip) communication
            route = 1 << (LEAD_CORE+6);
            mask = MCPL_2LEAD_MASK;
            for(i=0; i<N_CORE_WORKER; i++) {
                key = MCPL_2LEAD | (LEAD_CORE+i+1);
                rtr_mc_set(e++, key, mask, route);
            }

            // setup MCPL_FWD_IMG_INFO
            // the direction is :
            // - for chip x,y where x==y: link-1,3,5 and all cores internally
            // - for other chips: if x<y then go west, if x>y then go south
            // BUT, check if link is operational
            if(CHIP_X(sv->p2p_addr)==CHIP_Y(sv->p2p_addr)) {

                lanjutkan.....

            }
            else if(CHIP_X(sv->p2p_addr)<CHIP_Y(sv->p2p_addr)) {

            }
            else { // this is for (CHIP_X(sv->p2p_addr)>CHIP_Y(sv->p2p_addr))
                sv->link_en;
            }
        }
        return ok;
    }
}

/* In the beginning, the master core tries to discover how many workers
 * are available in the chip. For root master, it also tries to detect
 * how many nodes are in the system by reading p2p_table.
 * */
// inReservedChips() check if the given xy is in the reserved position
// return TRUE or FALSE
static uint inReservedChips(ushort xy)
{
    uint found = FALSE;
    ushort reserved[8] = {0x00, 0x10, 0x11, 0x01, 0x20, 0x30, 0x21, 0x31};
    for(ushort i=0; i<8; i++) {if(xy==reserved[i]) { found = TRUE; break; }}
    return found;
}

void netDiscovery(uint arg0, uint arg1)
{
    // give a delay, so that the workers are set up properly
    sark_delay_us(1000);

    // discover workers in a chip
    bcastFR(FRPL_BCAST_DISCOVERY, 0, NO_PAYLOAD);

    // for root master, also discovers available nodes in the machine
    if(sv->p2p_addr==ROOT_VIDPRO_NODE) {

        /* initialize nodeList
         * Note: we assume we only work with a few boards, not an entire big SpiNNaker machine
         * */
        uint dest, r;
        ushort dim = (sv->p2p_dims > 8) * (sv->p2p_dims & 0xF);

        nodeList = sark_alloc(dim, sizeof(ushort));
        if(nodeList==NUL) {
            io_printf(IO_STD, "[ERR] nodeList alloc!\n");
            rt_error(RTE_MALLOC);
        }

        // Note: Don't check the following chips:
        // <0,0>, <1,0>, <0,1>, <1,1>, <2,0>, <3,0>, <2,1>, <3,1>
        // they are reserved for jpeg enc/dec
        myNodeID=0;                 // root master
        nodeList[0] = sv->p2p_addr; // root master
        nNodes = 1;                 // root master

        // the following is a slow process, give additional delay for starting
        for(r=1; r<=0xFFFF; r++) {
            dest = rtr_p2p_get(r);
            if(dest != 6 && inReservedChips(r)==FALSE) {
                nodeList[nNodes++] = (ushort)r; // r can be extracted using CHIP_X() CHIP_Y()
            }
        }

        // then tell the chips about their node ID
        sdp_msg_t nodeIDmsg;
        nodeIDmsg.flags=7;
        nodeIDmsg.tag=0;
        nodeIDmsg.srce_addr=sv->p2p_addr;
        nodeIDmsg.srce_port=(SDP_INTERNAL_PORT<<5) | LEAD_CORE;
        nodeIDmsg.dest_port=nodeIDmsg.srce_port;
        nodeIDmsg.cmd_rc=SDP_CMD_RC_NODE_ID_INFO;
        nodeIDmsg.length=sizeof(sdp_hdr_t)+sizeof(cmd_hdr_t);
        for(r=1; r<nNodes; r++) {
            nodeIDmsg.seq=(ushort)r;
            nodeIDmsg.arg1=(ushort)nodeList[r];
            spin1_send_sdp_msg(&nodeIDmsg, 10);
        }
    }
}
