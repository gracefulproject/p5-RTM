/* This is the main file for the master cores */

#include "mVidPro.h"

void c_main()
{
    if(initApp()==SUCCESS) {
        if(initRouter()==SUCCESS) {
            spin1_schedule_callback(netDiscovery,0,0,PRIORITY_LOW);
            spin1_start(SYNC_NOWAIT);
        }
    }
}
