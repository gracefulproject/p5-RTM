#ifndef MVIDPRO_H
#define MVIDPRO_H

#include "../vidProNode.h"

/* Function prototypes */
uint initRouter();
void netDiscovery(uint arg0, uint arg1);
void bcastFR(uint key, uint payload, uint load_condition);

/* Variables */
uint nNodes;        // how many nodes
ushort myNodeID;
ushort *nodeList;


#endif // MVIDPRO_H

