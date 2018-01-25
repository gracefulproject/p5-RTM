#include "profiler.h"

void init_app()
{
  // initialize profiler ID, or create a list for it
  generateProfilerID(); // see profiler_util.c

  // initialize cpu load counter
  init_idle_cntr(); // see profiler_cpuload.cp

  // initialize router
  init_Router();    // see profiler_events.c

  // initialize event handler
  init_Handlers();  // see profiler_events.c

  // initialize PLL
  initPLL();
  // test:
  // showPLLinfo(sv->p2p_addr, 0);

  // initialize governor
  init_governor();

  // others:
  streaming = FALSE;    // by default we silent
  // put version to vcpu->user0 to be detected by host GUI
  sark.vcpu->user0 = PROFILER_VERSION;
}



void c_main ()
{
  sanityCheck();        // Since some features require strict conditions
  init_app();
  if(sv->p2p_addr==0)
	io_printf(IO_STD, "[INFO] Profiler-%d is ready!\n", my_pID);
  else
	io_printf(IO_BUF, "[INFO] Profiler-%d is ready!\n", my_pID);

  // wait for synchronization
  spin1_schedule_callback(startProfiling, 0, 0, SCHEDULED_PRIORITY_VAL);
  spin1_start(SYNC_WAIT);

  // if host tell to stop, then revert the PLL
  if(sv->p2p_addr==0)
	io_printf(IO_STD, "[INFO] Profiler-%d will be stopped!\n", my_pID);
  else
	io_printf(IO_BUF, "[INFO] Profiler-%d will be stopped!\n", my_pID);
  stopProfiling(0,0);
  revertPLL();
}

