#include "profiler.h"

/* The following is reminiscent of previous version:
#define lnIDTable					48
#define wdIDTable					2
static uchar profIDTable[lnIDTable][wdIDTable] = {
{0,0},{1,0},{2,0},{3,0},{4,0},
{0,1},{1,1},{2,1},{3,1},{4,1},{5,1},
{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},
{0,3},{1,3},{2,3},{3,3},{4,3},{5,3},{6,3},{7,3},
	  {1,4},{2,4},{3,4},{4,4},{5,4},{6,4},{7,4},
			{2,5},{3,5},{4,5},{5,5},{6,5},{7,5},
				  {3,6},{4,6},{5,6},{6,6},{7,6},
						{4,7},{5,7},{6,7},{7,7}};

*/


/*----------------------- Misc. Utilities ------------------------*/

/* Update:
 * generateProfilerID() now is based on rtr_p2p_get() from sark
 * Previously, it is defined using static profIDTable.
 * NOTE uint rtr_p2p_get (uint entry):
 *    Gets a P2P table entry. Returns a value in the range 0 to 7.
 *    \param entry the entry number in the range 0..65535
 *    \return table entry (range 0 to 7)
 *
 * The purpose of generageProfilerID is to identify orderly the
 * profiler chips. The process is:
 * 1. the root profiler will do generateProfilerID() and create a list
 *    of ID based on rtr_p2p_get() result.
 * 2. the root profiler will send the ID to the corresponding chip
 *    afterwards, each chip will report based on this ID
 * 3. the root profiler inform host-PC about this list so that the PC
 *    can interpret the incoming profiler data correctly.
*/

#define PID_LIST_MEM_LOC_DTCM 0
#define PID_LIST_MEM_LOC_SDRM 1
ushort *pID_list;   // profiler chip_list list, hByte=x, lByte=y
ushort nChips;		// number of chips in the system
uchar mem_type;     // 0=dtcm, 2=sdram (sysram is too small -> 32KB)
					// for a single or three board system, DTCM is used
					// otherwise, SDRAM will be used

void generateProfilerID()
{
	uint r;
	uint dest;
	ushort szX = sv->p2p_dims > 8;
	ushort szY = sv->p2p_dims & 0xF;
	ushort dim = szX * szY;
	if(dim <= 256)
		mem_type = PID_LIST_MEM_LOC_DTCM;
	else
		mem_type = PID_LIST_MEM_LOC_SDRM;

	// only root needs complete list of ID
	if(sv->p2p_addr == 0) {

		// is there any way we can interact with scamp to read how chip in
		// the system? Yes, use sv->p2p_dims

		// first, allocate ID-list buffer in memory
		switch (mem_type) {
		case PID_LIST_MEM_LOC_DTCM:
			pID_list = sark_alloc(dim, sizeof(uint));
			break;
		case PID_LIST_MEM_LOC_SDRM:
			pID_list = sark_xalloc(sv->sdram_heap, dim*sizeof(uint), 0, ALLOC_LOCK);
			break;
		}

		my_pID = 0;
		nChips = 1; // the root profiler
		pID_list[0] = sv->p2p_addr;
		// the following is a slow process
		for(r=1; r<=0xFFFF; r++) {
			dest = rtr_p2p_get(r);
			if(dest != 6) {
				pID_list[nChips] = (ushort)r; // r can be extracted using CHIP_X() CHIP_Y()
				nChips++;
			}
		}

#if(DEBUG_LEVEL>2)
		io_printf(IO_STD, "[INFO] Found %d-chips in the system\n", nChips);
		ushort XY;
		for(ushort i=0; i<nChips; i++) {
			XY = pID_list[i];
			io_printf(IO_BUF, "ID-%d is assigned to [%d,%d]\n",
					  i, CHIP_X(XY), CHIP_Y(XY));
		}
#endif

	}
	// for other profiler chip, just get its own ID
	else {
		nChips = 0;
		for(r=0; r<=0xFFFF; r++) {
			dest = rtr_p2p_get(r);
			if(dest != 6) {
				if(dest == 7) {
					my_pID = nChips;
					break;
				}
				else {
					nChips++;
				}
			}
		}
#if(DEBUG_LEVEL>0)
		io_printf(IO_BUF, "[INFO] My pID = %d\n", my_pID);
#endif
	}


    // then fill-in parts of profiler data
    myProfile.v = PROFILER_VERSION;
    myProfile.pID = my_pID;

	/* old version:
	short id=-1, i;

	uint x = CHIP_X(sv->p2p_addr);
	uint y = CHIP_Y(sv->p2p_addr);

	for(i=0; i<48; i++) {
		if(profIDTable[i][0]==x && profIDTable[i][1]==y) {
			id = i; break;
		}
	}
	return id;
	*/
}

void bCastStopMsg()
{
	// prepare sdp message
	sdp_msg_t bCast;
	bCast.flags = 7;
	bCast.tag = 0;
	bCast.srce_port = (DEF_INTERNAL_SDP_PORT << 5) + DEF_PROFILER_CORE;
	bCast.srce_addr = sv->p2p_addr;
	bCast.dest_port = bCast.srce_port;
	bCast.cmd_rc = HOST_TELL_STOP;
	bCast.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
	for(uint i=1; i<nChips; i++) {
		bCast.dest_addr = pID_list[i];
		spin1_send_sdp_msg(&bCast, DEF_TIMEOUT);
		sark_delay_us(500); // to avoid RTE
	}
	// then release the pID_list
	if(mem_type==PID_LIST_MEM_LOC_DTCM)
		sark_free(pID_list);
	else
		sark_xfree(sv->sdram_heap, pID_list, ALLOC_LOCK);
}

uchar getNumActiveCores()
{
	uchar i, nCores = sv->num_cpus, nApp = 0;
	for(i=0; i<nCores; i++) {
		if(sv->vcpu_base[i].cpu_state >= CPU_STATE_RUN &&
		   sv->vcpu_base[i].cpu_state < CPU_STATE_EXIT)
			nApp++;
	}
	return nApp; //nApp includes the monitor core and the profiler core!!!
}

void sanityCheck()
{
    if(sark_core_id() != DEF_PROFILER_CORE) {
        io_printf(IO_STD, "[ERR] Put me at core-%d!\n", DEF_PROFILER_CORE);
        rt_error(RTE_SWERR);
    }
    if(sark_app_id() != DEF_MY_APP_ID) {
        io_printf(IO_STD, "[ERR] Give me app id-%d!\n", DEF_MY_APP_ID);
        rt_error(RTE_SWERR);
    }
}
