#include "mVidPro.h"

void hSDP(uint mBox, uint port)
{
    sdp_msg_t *msg = (sdp_msg_t *)mBox;
    if(port==SDP_INTERNAL_PORT) {
        if(msg->cmd_rc==SDP_CMD_RC_NODE_ID_INFO)
            myNodeID = (ushort)msg->arg1;
    }
    spin1_msg_free(msg);
}


// bcastFR() is used mainly be LEAD_CORE to tell all working cores something
inline void bcastFR(uint key, uint payload, uint load_condition)
{
    rtr_fr_set(N_CORE_FOR_FWD_BITS);
    spin1_send_fr_packet(key, payload, load_condition);
}
