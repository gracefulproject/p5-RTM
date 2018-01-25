#include "wVidPro.h"

uint initApp()
{
    uint ok;
    wInfo.coreID = sark_core_id();
    if(wInfo.coreID==PROFILER_CORE || wInfo.coreID==LEAD_CORE) {
        io_printf(IO_STD, "[ERR] Wrong core for worker!\n");
        ok = FAILURE;
    }
    else if(sark_app_id()!=W_VIDPRO_NODE_APP_ID) {
        io_printf(IO_STD, "[ERR] Wrong app-id!\n");
        ok = FAILURE;
    }
    else {
        initHandlers();
        ok = SUCCESS;
    }
    return ok;
}

void initHandlers()
{
    spin1_callback_on(MCPL_PACKET_RECEIVED, hMCPL, PRIORITY_MCPL);
    spin1_callback_on(MC_PACKET_RECEIVED, hMC, PRIORITY_MCPL);
    spin1_callback_on(DMA_TRANSFER_DONE, hDMA, PRIORITY_DMA);
    spin1_callback_on(FRPL_PACKET_RECEIVED, hFRPL, PRIORITY_FRPL);
    spin1_callback_on(FR_PACKET_RECEIVED, hFR, PRIORITY_FRPL);
}
