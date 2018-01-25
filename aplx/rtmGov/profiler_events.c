#include <math.h>
#include "profiler.h"

// iptag
// IPTAG definitions from scamp:
#define IPTAG_NEW		0
#define IPTAG_SET		1
#define IPTAG_GET		2
#define IPTAG_CLR		3
#define IPTAG_TTO		4

//#define SDP_DEF_HOST_IP				    0x02F0A8C0	// 192.168.240.2, dibalik!


// sdp container
sdp_msg_t iptag;        // only for root profiler
sdp_msg_t reportMsg;	// for all profilers


/*------------------------- Other stuffs -------------------------------*/

#define TIMER_TICK_PERIOD_US        1000000



/*----------------------------------------------------------------------*/
/*----------------------- Function Prototypes --------------------------*/
void hSDP(uint mailbox, uint port);
void hMCPL(uint key, uint payload);
void hTimer(uint tick, uint Unused);
void initIPTag(uint hostIP, uint Unused);
void init_sdp_container();
/*
 * In order to set iptag properly, we need to collect information
 * from SCAMP about the ip address of host-PC. Usually tag-0 contains
 * valid IP address. Hence, before we initIPTag, we need to collect
 * host-PC IP address. The initiateIPTagReading should be scheduled.
 * */
void initiateIPTagReading(uint arg1, uint arg2);


/*----------------------------------------------------------------------*/
/*--------------------- Initialization function ------------------------*/

void init_Handlers()
{
    spin1_callback_on(SDP_PACKET_RX, hSDP, SCP_PRIORITY_VAL);
    spin1_callback_on(MCPL_PACKET_RECEIVED, hMCPL, MCPL_PRIORITY_VAL);
    spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
    spin1_callback_on(TIMER_TICK, hTimer, TIMER_PRIORITY_VAL);
    spin1_callback_on(USER_EVENT, collectData, SCHEDULED_PRIORITY_VAL);
    init_sdp_container();
    //initIPTag(); --> replaced with initiateIPTagReading()
    spin1_schedule_callback(initiateIPTagReading, 0, 0, 1);
}


/*----------------------------------------------------------------------*/
/*---------------------- Event handler function ------------------------*/

/* collectData is a USER_EVENT triggerd by hSlowTimer in profiler_cpuload.c
 * */
uchar virt_cpu;
void collectData(uint None, uint Unused)
{
	/*
#if(DEBUG_LEVEL>0)
	if(sv->p2p_addr==0)
		io_printf(IO_STD, "trig!\n");
#endif
*/
	// convert from phys to virt cpu
	for(uint i=0; i<18; i++) {
        virt_cpu = sv->p2v_map[i];	// the virtual cpu id
		virt_cpu_idle_cntr[virt_cpu] = stored_cpu_idle_cntr[i];
	}
	sark_mem_cpy((void *)&reportMsg.cmd_rc, (void *)&myProfile, sizeof(pro_info_t));
	//sark_mem_cpy((void *)reportMsg.data, (void *)stored_cpu_idle_cntr, 18);
	sark_mem_cpy((void *)reportMsg.data, (void *)virt_cpu_idle_cntr, 18);
#if(DEBUG_LEVEL>2)
	if(sv->p2p_addr==0) {
		if(streaming==FALSE)
			io_printf(IO_STD, "no streaming!\n");
		else
			io_printf(IO_STD, "do streaming!\n");
	}
	return;
#endif

	if(streaming==FALSE) return;
/*
#if(DEBUG_LEVEL>0)
	io_printf(IO_BUF, "Send!\n");
	return;
#endif
*/
	spin1_send_sdp_msg (&reportMsg, DEF_TIMEOUT);
}

void hTimer(uint tick, uint Unused)
{

}

void hMCPL(uint key, uint payload)
{
    /*
	uint subkey = key & 0xFFFFFF00;
	ushort senderID = key & 0x000000FF;
	if(subkey==MCPL_PROFILERS_REPORT_PART1) {
		sark_mem_cpy((void *)&proData[senderID], (void *)&payload, sizeof(uint));
	}
    */
}

void hSDP(uint mailbox, uint port)
{
     sdp_msg_t *msg = (sdp_msg_t *) mailbox;
#if(DEBUG_LEVEL>2)
	io_printf(IO_STD, "[INFO] sdp on port-%d\n", port);
#endif
    // use DEF_HOST_SDP_PORT for communication with the host via eth
    if(port==DEF_HOST_SDP_PORT) {
        switch(msg->cmd_rc) {
        case HOST_REQ_PLL_INFO:
			//spin1_schedule_callback(showPLLinfo, 1, 0, SCHEDULED_PRIORITY_VAL);
			//io_printf(IO_STD, "[INFO] HOST_REQ_PLL_INFO..\n");
			showPLLinfo(0, 0);
            break;
        case HOST_REQ_PROFILER_STREAM:
			// then use seq to determine: 0 stop, 1 start
			if(msg->seq==0) {
				io_printf(IO_STD, "[INFO] Streaming off...\n");
				streaming = FALSE;
			}
			else {
				io_printf(IO_STD, "[INFO] Streaming on...\n");
				streaming = TRUE;
			}
            break;
        case HOST_SET_FREQ_VALUE:
			{
            // seq structure: byte-1 component, byte-0 frequency
			PLL_PART comp = (PLL_PART)(msg->seq >> 8);
            uint f = msg->seq & 0xFF;
#if(DEBUG_LEVEL>2)
			io_printf(IO_STD, "[INFO] Request for comp-%d with f=%d\n", comp, f);
#endif
            changeFreq(comp, f);
            break;
			}
        case HOST_SET_GOV_MODE:
            /* change_governor() is called via hSDP
             * for GOV_USER, user_param will set the desired user_def_freq
             * for GOV_QLEARNING, user_param determines if Q-learning should be reset or not
             * */
            change_governor((gov_t)msg->seq, msg->arg1);
            break;
        case HOST_REQ_GOV_STATUS:
            spin1_schedule_callback(get_governor_status,0,0,SCHEDULED_PRIORITY_VAL);
            break;
		case HOST_TELL_STOP:
			spin1_exit(0);
		}
	}
    // use DEF_INTERNAL_SDP_PORT for internal (inter-chip) communication
    // such as with monitor core for SCP
    else if(port==DEF_INTERNAL_SDP_PORT){
        if(msg->cmd_rc==0x80){ // CMD_OK from previous request
			//hostIP = msg->arg1;
            spin1_schedule_callback(initIPTag, msg->arg1, 0, SCHEDULED_PRIORITY_VAL);
        }
		// root profiler might send this:
		if(msg->cmd_rc==HOST_TELL_STOP)
			spin1_exit(0);
	}
    spin1_msg_free (msg);
}

void init_Router()
{
    /*
    uint d, e, x, y, pc, dest;

    pc = 1 << (DEF_PROFILER_CORE+6); // profiler core

	x = CHIP_X(sv->p2p_addr);
	y = CHIP_Y(sv->p2p_addr);

	e = rtr_alloc(3);

	// broadcasting from root profiler:
	dest = pc;	
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

	rtr_mc_set(e, MCPL_BCAST_REQ_UPDATE, 0xFFFFFFFF, dest); e++;

	// report towards root profiler in core <0,0>
	if (x>0 && y>0)			dest = (1 << 4);	// south-west
	else if(x>0 && y==0)	dest = (1 << 3);	// west
	else if(x==0 && y>0)	dest = (1 << 5);	// south
	else					dest = pc;
    // use lower 2-bytes for profiler ID
	rtr_mc_set(e, MCPL_PROFILERS_REPORT_PART1, 0xFFFFFF00, dest); e++;
	rtr_mc_set(e, MCPL_PROFILERS_REPORT_PART2, 0xFFFFFF00, dest); e++;
    */
}



void init_sdp_container()
{
    // for basic report: temperature, freq, numActiveCore, etc
    reportMsg.tag = DEF_REPORT_TAG;                      // Don't forget to specify the proper iptag in ybug
    reportMsg.dest_port = PORT_ETH;                        // Ethernet
    reportMsg.dest_addr = sv->eth_addr;                    // Nearest Ethernet chip
    reportMsg.flags = 0x07;                                // no need for reply
    reportMsg.srce_port = (DEF_HOST_SDP_PORT << 5) + (uchar)sark_core_id();
    reportMsg.srce_addr = sv->p2p_addr;
    reportMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t) + (18*sizeof(uchar));

#if(DEBUG_LEVEL>2)
	if(sv->p2p_addr==0) {
		io_printf(IO_STD, "sizeof(pro_info_t) = %d\n", sizeof(pro_info_t));
		io_printf(IO_STD, "report.dest_port = 0x%x\n", reportMsg.dest_port);
		io_printf(IO_STD, "report.dest_addr = 0x%x\n", reportMsg.dest_addr);
		io_printf(IO_STD, "report.srce_port = 0x%x\n", reportMsg.srce_port);
		io_printf(IO_STD, "report.srce_addr = 0x%x\n", reportMsg.srce_addr);
		io_printf(IO_STD, "total paket length = %d\n", reportMsg.length);
	}
#endif

    // prepare IPtagging mechanism, only if I'm root
	if(sv->p2p_addr==0) {
		iptag.flags = 0x87;	// initially needed for iptag triggering
        iptag.tag = 0;		// internal
        iptag.srce_addr = sv->p2p_addr;
        iptag.srce_port = (DEF_INTERNAL_SDP_PORT << 5) + (uchar)sark_core_id();	// use port-7
        iptag.dest_addr = sv->p2p_addr;
		iptag.dest_port = 0;				// send to "root" (scamp)
        iptag.cmd_rc = CMD_IPTAG;
        iptag.arg1 = (IPTAG_GET << 16) + 0; // target iptag-0
        iptag.arg2 = 1; //remember, scamp use this for some reason
        iptag.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
    }
}

void initiateIPTagReading(uint arg1, uint arg2)
{
    // only root profiler needs this
    // the result should be read through hSDP
    if(sv->p2p_addr==0) {
#if(DEBUG_LEVEL>2)
		io_printf(IO_STD, "[INFO] Initiate iptagging...\n");
#endif
        spin1_send_sdp_msg(&iptag, DEF_TIMEOUT);
    }
}


void initIPTag(uint hostIP, uint Unused)
{
    // only chip <0,0>
    if(sv->p2p_addr==0) {
#if(DEBUG_LEVEL>2)
		io_printf(IO_STD, "[INFO] Setting iptag-%d for port-%d\n",
				  DEF_REPORT_TAG, DEF_REPORT_PORT);
#endif
        // set the report tag
		iptag.flags = 7;
        iptag.arg1 = (IPTAG_SET << 16) + DEF_REPORT_TAG;
        iptag.arg2 = DEF_REPORT_PORT;
        //iptag.arg3 = SDP_DEF_HOST_IP;
        iptag.arg3 = hostIP;
        spin1_send_sdp_msg(&iptag, DEF_TIMEOUT);
#if(DEBUG_LEVEL>2)
		io_printf(IO_STD, "[INFO] Setting iptag-%d for port-%d\n",
				  DEF_ERR_INFO_TAG, DEF_ERR_INFO_PORT);
#endif
		// set the generic error info tag (for debugging)
        iptag.arg1 = (IPTAG_SET << 16) + DEF_ERR_INFO_TAG;
        iptag.arg2 = DEF_ERR_INFO_PORT;
        spin1_send_sdp_msg(&iptag, DEF_TIMEOUT);
    }
}
