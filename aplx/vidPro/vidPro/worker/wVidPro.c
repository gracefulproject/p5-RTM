/* This is the main file for the worker cores */

#include "wVidPro.h"

void c_main()
{
    if(initApp()==SUCCESS) {
        spin1_start(SYNC_NOWAIT);
    }
}
