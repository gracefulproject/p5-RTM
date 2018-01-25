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

// the following two variables are just for debugging
uint myTick = 0;
uint tick_cnt = 0;

INT_HANDLER hSlowTimer (void)
{
  uint r25, i;

  // found problem here: solution &=, not =
  dma[DMA_GCTL] &= 0x7FFFFFFF; // clear bit[31] of dma GCTL

  //try this:
  vic[VIC_SOFT_CLR] = 1 << SLOW_CLK_INT;

#if(DEBUG_LEVEL>2)
  if(tick_cnt < 100) {
	  if(++myTick >= 10000) {
		//if run too long, it make RTE:
		io_printf(IO_BUF, "tick-%d, nAct=%d, temp=%d, fcpu=%dMHz\n",
				  tick_cnt++, myProfile.nActive, myProfile.temp3, myProfile.cpu_freq);
		myTick = 0;
	  }
  }
#else
  if(++idle_cntr_cntr>100) {
	// copy to stored_cpu_idle_cntr
	// sark_mem_cpy(stored_cpu_idle_cntr, running_cpu_idle_cntr, 18);
	// clear running_cpu_idle_cntr and idle_cntr_cntr
	for(int i=0; i<18; i++) {
	  stored_cpu_idle_cntr[i] = 100 - running_cpu_idle_cntr[i];
	  running_cpu_idle_cntr[i] = 0;
	}
	idle_cntr_cntr = 0;
	// trigger software interrupt for the main profiler loop
	// the software interrupt is handled via user event
	spin1_trigger_user_event(0, 0);
	//spin1_schedule_callback(collectData, 0, 0, SCHEDULED_PRIORITY_VAL);
  }
  else {
	// let's split the work:
	// when counter = 94, read temperature
	if(idle_cntr_cntr==94) {
	  myProfile.temp3 = readTemp();
	  myProfile.temp1 = tempVal[0];
	}

	// when counter = 96, read frequency
	// by calling readFreq, all parameters in pll are read (eg. Sfreq, Sdiv, etc.)
	if(idle_cntr_cntr==96)
		myProfile.cpu_freq = readFreq(&myProfile.ahb_freq, &myProfile.rtr_freq);

	// when counter = 98, read nActiveCores
	if(idle_cntr_cntr==98)
	  myProfile.nActive = getNumActiveCores();

	// detect idle state of all cores
	r25 = sc[SC_SLEEP];
	for(i=0; i<18; i++)
		running_cpu_idle_cntr[i] += (r25 >> i) & 1;
  }

#endif
  vic[VIC_VADDR] = (uint) vic;			// Tell VIC we're done
}

