#include "packets.h"
#include "packet_defs.h"
#include "SensorDB.h"
#include "logger.h"
#include "sensor.h"
#include "crc16.h"
#include "curl.h"
#include "settings.h"
#include "sensor.h"
#include "XBeeCommunicator.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>

#include <map>

#define PACKETS_DEBUG

using namespace std;

#define ZEROC_INKELVIN 273.15

// Some ugly externs if there ever were any.  WHEW!  UGLY UGLY UGLY UGLY
// UGLY CODE WARNING!!!!!
extern FILE* __stdout_log;

extern void hexdump(void*, int);
extern XBeeAddress TransformTo8ByteAddress(XBeeAddress_7Bytes);
extern string GetXBeeID(XBeeAddress*);

void HandlePacketRev0(XBeeCommunicator* comm, Frame* apiFrame);
void HandlePacketRev1(XBeeCommunicator* comm, Frame* apiFrame);

/**
	HandlePacket callbacks do not need to do any verification, that is already done.
	The only thing they need to do is parse the packet and reply if needed.
**/
typedef void (*HandlePacketCallback)(XBeeCommunicator*, Frame*);

typedef struct {
	HandlePacketCallback handlePacketCallback;
	int minRev;
	int maxRev;
} HandlePacketCallbackStruct;

HandlePacketCallbackStruct handlePacketCallbacks[] = {
	{HandlePacketRev0, 0, 0},
	{HandlePacketRev1, 1, 1}
};

int handlePacketCallbacksLength = sizeof(handlePacketCallbacks) / sizeof(*handlePacketCallbacks);

void SendPacket(XBeeCommunicator* comm, int revision, XBeeAddress* address, void* packet, int id) {
	int size;
	switch(revision) {
		case REVISION_0:
			size = sizeof(PacketRev0);
		break;

		case REVISION_1:
			size = sizeof(PacketRev1);
		break;
	}
	

	XBAPI_Transmit(comm, address, packet, size);
}

void SendReceiverAddress(XBeeCommunicator* comm, XBeeAddress* dest, int id) {
	fprintf(__stdout_log, "Warning:  SendReceiverAddress is deprecated.\n");

	#ifdef PACKETS_DEBUG
	for(int i=0; i<sizeof(XBeeAddress); i++) {
		fprintf(__stdout_log, "%x%c", dest->addr[i], (i==sizeof(XBeeAddress)-1) ? '\n' : ' ');
	}
	#endif
	
	PacketRev0 packet_buffer;
	memset(&packet_buffer, 0, sizeof(PacketRev0));

	packet_buffer.header.command = REQUEST_RECEIVER;
	packet_buffer.header.magic = 0xAA55;
	packet_buffer.header.revision = PROGRAM_REVISION;
	packet_buffer.header.crc16 = CRC16_Generate((byte*)&packet_buffer, sizeof(PacketRev0));

	SendPacket(comm, REVISION_0, dest, &packet_buffer, id);
}

void HandlePacket(XBeeCommunicator* comm, Frame* apiFrame) {
	//fprintf(__stdout_log, "HELP US ALL!\n");

	int revision = 0;

	// We have to do this to get the revision.
	PacketRev0* packet = &apiFrame->rx.rev0.packet;

	// Right now, .revision is at the same location for all revisions, thus we can just do this:
	revision = packet->header.revision;

	int i;
	for(i=0; i < handlePacketCallbacksLength; i++) {
		HandlePacketCallbackStruct* cb = &handlePacketCallbacks[i];
		// This finds a HandlePacket for a specific revision.
		if(revision >= cb->minRev && revision <= cb->maxRev) {
			cb->handlePacketCallback(comm, apiFrame);
		}
	}

	/*switch(packet->header.command) {
		case REQUEST_RECEIVER: {
			#ifdef PACKETS_DEBUG
			fprintf(__stdout_log, "receiver request\n");
			#endif
			
			XBeeAddress xbee_addr;
			xbee_addr = TransformTo8ByteAddress(apiFrame->rx.source_address); // Because of some weird design oversight on the xbee engineer's part, I have to such things as this.

			SendReceiverAddress(port, &xbee_addr);
			
			if(GetSensorMap()[xbee_addr] == NULL) {
				AddSensor(&xbee_addr); // It should be adding here, but it's not.  It's waiting till the report.  FIXME
			}
			
			#ifdef PACKET_DEBUG
			LogEntry entry;
			entry.sensorId = xbee_addr;
			entry.time = time(NULL);
			
			Logger_AddEntry(&entry, REQUEST_RECEIVER);
			#endif
			
			SensorUpdate(&xbee_addr); // This will update the receiver struct and let the other thread know not to throw a fit.
		} break;

		case REPORT: {
			//hexdump(packet, 32);
			int resistance = packet->report.thermistorResistance;
			int res25C = packet->report.thermistorResistance25C;
			int probeBeta = packet->report.thermistorBeta;
			
			double probe0_temp = 1.0 / ((log((double)resistance / res25C) / probeBeta)+(1/(ZEROC_INKELVIN+25))) - ZEROC_INKELVIN;
			
			#ifdef PACKET_DEBUG_VERBOSE0
			fprintf(__stdout_log, "probe0_temp=%f\n", probe0_temp);
			#endif
			
			// We get to do this because some guy thought it was a brilliant idea to use 7 byte addresses instead of 8 byte on the receive indicator.
			XBeeAddress address;
			address = TransformTo8ByteAddress(apiFrame->rx.source_address);
			
			#ifdef PACKET_DEBUG_VERBOSE0
			LogEntry entry;
			entry.resistance = packet->report.thermistorResistance;

			entry.sensorId = address;
			entry.time = time(NULL);
			entry.xbee_reset = apiFrame->rx.packet.report.xbee_reset;

			Logger_AddEntry(&entry, packet->header.command);
			#endif
			
			if(GetSensorMap()[address] == NULL) {
				AddSensor(&address);
			}
			
			// Make sure our internal representation of this 
			// sensor stays alive.
			SensorUpdate(&address);
			
			// Add report.
			SensorDB db;
			db.AddReport(GetXBeeID(&address), time(NULL), probe0_temp);
		} break;
	}*/
}

extern uint32 swap_endian_32(uint32);

uint64 swap_endian_64(uint64 u) {
	uint64 n0, n1;
	n0 = (u >> 56) | ((u >> 40) & 0xFF00) | ((u >> 24) & 0xFF0000) | ((u >> 8) & 0xFF000000);
	n1 = (u << 56) | ((u & 0xFF00) << 40) | ((u & 0xFF0000) << 24) | ((u & 0xFF000000) << 8);
	return n0 | n1;
}

void HandlePacketRev1(XBeeCommunicator* comm, Frame* apiFrame) {
	PacketRev1* packet = &apiFrame->rx.rev1.packet;
	
	#ifdef PACKETS_DEBUG_VERBOSE0
	hexdump(apiFrame, sizeof(Frame));
	#endif
	
	// Do our CRC16 compare here.
	unsigned short crc16 = packet->header.crc16;
	packet->header.crc16 = 0;
	
	unsigned short calc_crc16 = CRC16_Generate((unsigned char*)packet, sizeof(PacketRev1));
	
	if(calc_crc16 != crc16) {
		fprintf(__stdout_log, "CRC16 hashes do not match.  %x!=%x, Discarding. sizeof(Packet)=%x\n", calc_crc16, crc16, sizeof(PacketRev1));
		hexdump(packet, sizeof(PacketRev1));
		return;
	}

	switch(packet->header.command) {
		case REQUEST_RECEIVER: {
			XBeeAddress xbee_addr;
			xbee_addr = TransformTo8ByteAddress(apiFrame->rx.rev1.source_address);

			SensorId sensorId = packet->header.sensorId;
			long n = 0;
			n = ~n;

			if(sensorId.uId == n) {
				// Get a new sensor ID.
				SimpleCurl* curl = new SimpleCurl();
				string url = Settings::get("server") + string("/api/getid");
				CURLBuffer* buf = curl->get(url, "");
				
				if(buf == NULL) {
					fprintf(__stdout_log, "Something went wrong.  buf should not be null.\n");
				} else {
					sensorId.uId = swap_endian_64(strtoul(buf->buffer, NULL, 16));
				}

				fprintf(__stdout_log, "server=%s, sensorId=%lx\n", url.c_str(), swap_endian_64(sensorId.uId));
				if(buf != NULL) {
					delete buf;
				}
				
				delete curl;
			}

			PacketRev1* reply = new PacketRev1;
			memset(reply, 0, sizeof(PacketRev1));
			reply->header.flags = 0;

			// We set it to the packet header's revision, in case we ever support multiple
			// revisions with one callback.
			reply->header.revision = packet->header.revision;
			reply->header.command = RECEIVER_ACK;
			reply->header.sensorId = sensorId;
			hexdump(reply, sizeof(PacketRev1));
			reply->header.crc16 = CRC16_Generate((byte*)&packet, sizeof(PacketRev1));
			
			SendPacket(comm, REVISION_1, &xbee_addr, reply, apiFrame->rx.rev0.frame_id);

			if(GetSensorMap()[sensorId] == NULL) {
				AddSensor(&sensorId);
			}
		}
		case TEMP_REPORT: {
			uint32* probeResistances = packet->tempReport.probeResistances;
			uint16 probeBeta = packet->tempReport.probeBeta;
			uint32 res25C = packet->tempReport.probeResistance25C;
			
			int i;
			
			double probeTemps[NUM_PROBES];

			for(i=0; i < NUM_PROBES; i++) {
				probeTemps[i] = 1.0 / ((log((double)probeResistances[i] / res25C) / probeBeta)+(1/(ZEROC_INKELVIN+25))) - ZEROC_INKELVIN;
				if(probeResistances[i] == 0) {
					probeTemps[i] = nan("");
				}
			}
	
			// We get to do this because some guy thought it was a brilliant idea to use 7 byte addresses instead of 8 byte on the receive indicator.
			XBeeAddress address;
			SensorId id = packet->header.sensorId;
			address = TransformTo8ByteAddress(apiFrame->rx.rev1.source_address);
		

			uint64 n = 0;
			n = ~n;
			if(id.uId == n) {
				// Get a new sensor ID.
				SimpleCurl* curl = new SimpleCurl();
				string url = Settings::get("server") + string("/api/getid");
				CURLBuffer* buf = curl->get(url, "");
				
				if(buf == NULL) {
					fprintf(__stdout_log, "warning:  Something went wrong.  buf should not be null.\n");
				} else {
					id.uId = swap_endian_64(strtoul(buf->buffer, NULL, 16));
				}

				fprintf(__stdout_log, "server=%s, sensorId=%lx\n", url.c_str(), swap_endian_64(id.uId));

				if(buf != NULL) {
					delete buf;
				}
				delete curl;
			}

			#ifdef PACKET_DEBUG_VERBOSE0
			LogEntry entry;
			entry.resistance = packet->report.thermistorResistance;

			entry.sensorId = id;
			entry.time = time(NULL);
			entry.xbee_reset = apiFrame->rx.packet.report.xbee_reset;

			Logger_AddEntry(&entry, packet->header.command);
			#endif
			
			if(GetSensorMap()[id] == NULL) {
				AddSensor(&id);
			}
			
			// Make sure our internal representation of this 
			// sensor stays alive.
			SensorUpdate(&id);
			
			// Add report.
			SensorDB db;	
			db.AddReport(GetID(&id), time(NULL), probeTemps, 6.5);

			hexdump(packet, sizeof(PacketRev1));

		} break;
	}
}

void HandlePacketRev0(XBeeCommunicator* comm, Frame* apiFrame) {
	PacketRev0* packet = &apiFrame->rx.rev0.packet;
	
	#ifdef PACKETS_DEBUG_VERBOSE0
	hexdump(apiFrame, sizeof(Frame));
	#endif
	
	// Do our CRC16 compare here.
	unsigned short crc16 = packet->header.crc16;
	packet->header.crc16 = 0;
	
	unsigned short calc_crc16 = CRC16_Generate((unsigned char*)packet, sizeof(PacketRev0));
	
	if(calc_crc16 != crc16) {
		fprintf(__stdout_log, "CRC16 hashes do not match.  %x!=%x, Discarding. sizeof(Packet)=%x\n", calc_crc16, crc16, sizeof(PacketRev0));
		hexdump(packet, sizeof(PacketRev0));
		return;
	}

	switch(packet->header.command) {
		case REQUEST_RECEIVER: {
			#ifdef PACKETS_DEBUG
			fprintf(__stdout_log, "receiver request\n");
			#endif
			
			XBeeAddress xbee_addr;
			xbee_addr = TransformTo8ByteAddress(apiFrame->rx.rev0.source_address); // Because of some weird design oversight on the xbee engineer's part, I have to such things as this.

			SendReceiverAddress(comm, &xbee_addr, apiFrame->rx.rev0.frame_id);
			
			SensorId tempId;
			tempId.uId = xbee_addr.uAddr;

			if(GetSensorMap()[tempId] == NULL) {
				AddSensor(&tempId); // It should be adding here, but it's not.  It's waiting till the report.  FIXME
			}
			
			#ifdef PACKET_DEBUG
			LogEntry entry;
			entry.sensorId = xbee_addr;
			entry.time = time(NULL);
			
			Logger_AddEntry(&entry, REQUEST_RECEIVER);
			#endif
			
			SensorUpdate(&tempId); // This will update the receiver struct and let the other thread know not to throw a fit.
		} break;

		case REPORT: {
			//hexdump(packet, 32);
			int resistance = packet->report.thermistorResistance;
			int res25C = packet->report.thermistorResistance25C;
			int probeBeta = packet->report.thermistorBeta;
			
			double probe0_temp = 1.0 / ((log((double)resistance / res25C) / probeBeta)+(1/(ZEROC_INKELVIN+25))) - ZEROC_INKELVIN;
			
			#ifdef PACKET_DEBUG_VERBOSE0
			fprintf(__stdout_log, "probe0_temp=%f\n", probe0_temp);
			#endif
			
			// We get to do this because some guy thought it was a brilliant idea to use 7 byte addresses instead of 8 byte on the receive indicator.
			XBeeAddress address;
			SensorId id;
			address = TransformTo8ByteAddress(apiFrame->rx.rev0.source_address);
			id.uId = address.uAddr;

			#ifdef PACKET_DEBUG_VERBOSE0
			LogEntry entry;
			entry.resistance = packet->report.thermistorResistance;

			entry.sensorId = id;
			entry.time = time(NULL);
			entry.xbee_reset = apiFrame->rx.packet.report.xbee_reset;

			Logger_AddEntry(&entry, packet->header.command);
			#endif
			
			if(GetSensorMap()[id] == NULL) {
				AddSensor(&id);
			}
			
			// Make sure our internal representation of this 
			// sensor stays alive.
			SensorUpdate(&id);
			
			// Add report.
			SensorDB db;
			db.AddReport(GetXBeeID(&address), time(NULL), probe0_temp);
		} break;
	}
}
