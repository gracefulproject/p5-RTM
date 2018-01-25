#ifndef VIDPRONODE_H
#define VIDPRONODE_H

/* This is common header file used by both master and worker cores in vidpro nodes */

#include "../vidPro.h"
#include <spin1_api.h>

#define DEBUG_LEVEL 1

/* Generic Function prototypes */
uint initApp();         // includes sanity check
void initHandlers();

/* Event handler */
void hDMA   (uint tid,  uint tag);
void hSDP   (uint mBox, uint port);
void hMC    (uint key,  uint null);
void hMCPL  (uint key,  uint payload);
void hFR    (uint key,  uint null);
void hFRPL  (uint key,  uint payload);

/* Generic variables */
w_info_t wInfo;


#endif

