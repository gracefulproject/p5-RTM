/* The idea for cpu utilization using slow timer:
 * - Slow timer runs at about 32kHz. Whenever it fires, it detects
 *   the sleep state of each cores.
 * - After 100 times counting, it generates software interrupt.
 * - This 100 limit equivalent to 100%. Hence, if a core counter has 
 *   the value of 100, it means that core is 100% utilized during the
 *   sampling period of 1000000/32000 us == 3.124ms
 */
#include "spin1_api.h"
extern uchar running_cpu_idle_cntr[18]; // an array of 18-cores idle counters
extern uchar stored_cpu_idle_cntr[18];
extern uint idle_cntr_cntr; // counter of idle counter
extern void print_cntr(uint null, uint nill);

INT_HANDLER hSlowTimer (void)
{
  uint r25, i;

  dma[DMA_GCTL] = 0x7FFFFFFF; // clear bit[31] of dma GCTL

  if(++idle_cntr_cntr>=100) {
    // copy to stored_cpu_idle_cntr
    // sark_mem_cpy(stored_cpu_idle_cntr, running_cpu_idle_cntr, 18);
    // clear running_cpu_idle_cntr and idle_cntr_cntr
    for(int i=0; i<18; i++) {
      stored_cpu_idle_cntr[i] = 100 - running_cpu_idle_cntr[i];
      running_cpu_idle_cntr[i] = 0;
    }
    idle_cntr_cntr = 0;
    // trigger software interrupt for the main profiler loop
    spin1_trigger_user_event(0,0);
  }
  else {
    // detect idle state of all cores
    r25 = sc[SC_SLEEP];
    for(i=0; i<18; i++)
        running_cpu_idle_cntr[i] += (r25 >> i) & 1;
  }
  vic[VIC_VADDR] = (uint) vic;			// Tell VIC we're done
}

/* hSoftInt() is triggered when idle_cntr_cntr in hSlowTimer()
 * reaches 100. This signify ends of sampling period.
 * profiler should response this to collect the cpu utilization
 * value.
 * Will not be used, since we use user event.
 */
INT_HANDLER hSoftInt (void)
{
  vic[VIC_VADDR] = (uint) vic;
}
