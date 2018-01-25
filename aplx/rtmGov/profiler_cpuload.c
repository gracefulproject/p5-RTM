/* The idea for cpu utilization using slow timer:
 * - Slow timer runs at about 32kHz. Whenever it fires, it detects
 *   the sleep state of each cores.
 * - After 100 times counting, it generates software interrupt.
 * - This 100 limit equivalent to 100%. Hence, if a core counter has 
 *   the value of 100, it means that core is 100% utilized during the
 *   sampling period of 1000000/32000 us == 3.124ms
 */
//#include <sark.h>
#include "profiler.h"

// print_cntr just for debugging
// will be called/scheduled by hSoftInt
void print_cntr(uint null, uint nill)
{
  //for(int i=0; i<18; i++) {
    //io_printf(IO_STD,"phys_cpu-%d = %d\n", i, stored_cpu_idle_cntr[i]);
    io_printf(IO_STD, "%03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d\n"
                    , stored_cpu_idle_cntr[0]
                    , stored_cpu_idle_cntr[1]
                    , stored_cpu_idle_cntr[2]
                    , stored_cpu_idle_cntr[3]
                    , stored_cpu_idle_cntr[4]
                    , stored_cpu_idle_cntr[5]
                    , stored_cpu_idle_cntr[6]
                    , stored_cpu_idle_cntr[7]
                    , stored_cpu_idle_cntr[8]
                    , stored_cpu_idle_cntr[9]
                    , stored_cpu_idle_cntr[10]
                    , stored_cpu_idle_cntr[11]
                    , stored_cpu_idle_cntr[12]
                    , stored_cpu_idle_cntr[13]
                    , stored_cpu_idle_cntr[14]
                    , stored_cpu_idle_cntr[15]
                    , stored_cpu_idle_cntr[16]
                    , stored_cpu_idle_cntr[17]);
  //}
  //io_printf(IO_STD,"\n\n");
}

extern INT_HANDLER hSlowTimer (void);
void init_idle_cntr()
{
	for(uint i=0; i<18; i++) {
		running_cpu_idle_cntr[i] = 0;
		stored_cpu_idle_cntr[i] = 0;
	}
	idle_cntr_cntr = 0; //master counter that count up to 100
	//sark_vic_set (SLOT_FIQ, SLOW_CLK_INT, 1, hSlowTimer);
	//sark_vic_set (SLOT_10, SLOW_CLK_INT, 1, hSlowTimer);

	if(vic[VIC_SELECT]) {
#if(DEBUG_LEVEL>2)
		io_printf(IO_BUF, "VIC_SELECT = 0x%x\n", vic[VIC_SELECT]);
		io_printf(IO_BUF, "Using IRQ for idle-cntr\n");
#endif
		sark_vic_set (SLOT_10, SLOW_CLK_INT, 0, hSlowTimer); // don't enable it yet? wait for synchronization
	}
	else {
#if(DEBUG_LEVEL>2)
		io_printf(IO_BUF, "Using FIQ for idle-cntr\n");
#endif
		sark_vic_set (SLOT_FIQ, SLOW_CLK_INT, 0, hSlowTimer);
	}

}

uint _vic;

// startProfiling is scheduled in the c_main and waits synchronization signal
void startProfiling(uint null, uint nill)
{
	_vic = vic[VIC_ENABLE];
	vic[VIC_ENABLE] = 1 << SLOW_CLK_INT;
}

// startProfiling is scheduled in the c_main and waits synchronization signal
extern void bCastStopMsg();	// this is defined in profiler_util.c
void stopProfiling(uint null, uint nill)
{
	vic[VIC_ENABLE] = _vic;
	// then tell the rest of the system to stop as well
	if(sv->p2p_addr==0)
		bCastStopMsg();
}
