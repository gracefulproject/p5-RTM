/* This is the working version using SLOW_CLK_INT with spin1_api.
 * Key: using SLOT10 (or any free slot)
 */
#include <sark.h>
#include <spin1_api.h>

uchar running_cpu_idle_cntr[18];
uchar  stored_cpu_idle_cntr[18];
uint idle_cntr_cntr = 0; //master counter that count up to 100

uint tick = 0;
uint tick_cnt = 0;

extern INT_HANDLER hSlowTimer (void);

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

void init_app()
{
  // initialize counter
  for(int i=0; i<18; i++) {
    running_cpu_idle_cntr[i] = 0;
    stored_cpu_idle_cntr[i]  = 0;
  }
  // initialize router
  
}

void c_main ()
{
  init_app();
  io_printf (IO_STD, "Try cpu load counter\n");
  sark_vic_set (SLOT_10, SLOW_CLK_INT, 1, hSlowTimer);
  spin1_callback_on(USER_EVENT, print_cntr, 1);
  spin1_start(SYNC_NOWAIT);
}
