#include "wVidPro.h"

void hFR(uint key, uint null)
{
    if(key==FRPL_BCAST_DISCOVERY) {
        spin1_send_mc_packet(MCPL_2LEAD | wInfo.coreID, 0, NO_PAYLOAD);
    }
}
