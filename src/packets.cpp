#include "packets.h"
#include "packet_defs.h"
#include "SensorDB.h"
#include "logger.h"
#include "sensor.h"
#include "crc16.h"

#include <cstdio>
#include <cmath>
#include <cstring>

#include <map>

//#define PACKETS_DEBUG

using namespace std;

#define ZEROC_INKELVIN 273.15

// Some ugly externs if there ever were any.  WHEW!  UGLY UGLY UGLY UGLY
// UGLY CODE WARNING!!!!!
extern FILE* __stdout_log;

extern void hexdump(void*, int);
extern XBeeAddress TransformTo8ByteAddress(XBeeAddress_7Bytes);
extern string GetXBeeID(XBeeAddress*);

void SendPacket(SerialPort* port, XBeeAddress* address, Packet* packet) {
	XBAPI_Transmit(port, address, packet, sizeof(Packet));
}

void SendReceiverAddress(SerialPort* port, XBeeAddress* dest) {
	#ifdef PACKETS_DEBUG
	for(int i=0; i<sizeof(XBeeAddress); i++) {
		fprintf(__stdout_log, "%x%c", dest->addr[i], (i==sizeof(XBeeAddress)-1) ? '\n' : ' ');
	}
	#endif
	
	Packet packet_buffer;
	memset(&packet_buffer, 0, sizeof(Packet));

	packet_buffer.header.command = REQUEST_RECEIVER;
	packet_buffer.header.magic = 0xAA55;
	packet_buffer.header.revision = PROGRAM_REVISION;
	packet_buffer.header.crc16 = CRC16_Generate((byte*)&packet_buffer, sizeof(Packet));

	SendPacket(port, dest, &packet_buffer);
}

void HandlePacket(SerialPort* port, Frame* apiFrame) {
	//fprintf(__stdout_log, "HELP US ALL!\n");

	Packet* packet = &apiFrame->rx.packet;
	
	#ifdef PACKETS_DEBUG_VERBOSE0
	hexdump(apiFrame, sizeof(Frame));
	#endif
	
	// Do our CRC16 compare here.
	unsigned short crc16 = packet->header.crc16;
	packet->header.crc16 = 0;
	
	unsigned short calc_crc16 = CRC16_Generate((unsigned char*)packet, sizeof(Packet));
	
	if(calc_crc16 != crc16) {
		fprintf(__stdout_log, "CRC16 hashes do not match.  %x!=%x, Discarding. sizeof(Packet)=%x\n", calc_crc16, crc16, sizeof(Packet));
		return;
	}
	
	switch(packet->header.command) {
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
	}
}


