#ifndef CHIPFWDR_H
#define CHIPFWDR_H

#include <spin1_api.h>

#include "../vidPro.h"

#define DEBUG_LEVEL 1

#define N_CORE_FOR_FWD  5

/* Initialization */
uint initRouter();
uint initApp();         // includes sanity check
void initHandlers();
void initIPtag(uint phase, uint hostIP);

/* Event handler */
void hSDP  (uint mBox, uint port);
void hMCPL (uint key,  uint payload);
void hMC   (uint key,  uint null);
void hDMA  (uint tid,  uint tag);
void hFRPL (uint key,  uint payload);

/* Variables */
uint coreID;

#endif
