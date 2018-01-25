#include "gateway.h"

void c_main()
{
    if(initApp()==SUCCESS) {    // check if we're in right chip and core
        if(wInfo.coreID==LEAD_CORE){
            if(initRouter()==FAILURE)
                rt_error(RTE_PKT);
        }
        initHandlers();
        spin1_start(SYNC_NOWAIT);       
    }
    else {
#if(DEBUG_LEVEL>0)
        if(wInfo.coreID==LEAD_CORE)
            io_printf(IO_STD, "[ERR] Cannot start gateway!\n");
        else
            io_printf(IO_BUF, "[ERR] Cannot start gateway!\n");
#endif
    }
}

