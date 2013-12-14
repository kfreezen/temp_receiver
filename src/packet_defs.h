#ifndef PACKET_DEFS_H
#define PACKET_DEFS_H
#include "packets.h"
#include "xbee.h"

void HandlePacket(XBeeCommunicator* comm, Frame* frame);

#endif
