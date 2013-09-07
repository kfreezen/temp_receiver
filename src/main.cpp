#include <string>
#include <iostream>
#include <pthread.h>
#include <map>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <unistd.h>

#include "xbee.h"
#include "globaldef.h"
#include "serial.h"
#include "logger.h"
#include "SensorDB.h"

using namespace std;

typedef struct {
	XBeeAddress addr;
	int lastPacketTime;
} Sensor;

// I feel dirty using these globals, but alas, I'm too lazy to do anything else.

map<XBeeAddress, Sensor*> sensorMap;
XBeeAddress receiver_addr;

// END GLOBALS

void AddSensor(XBeeAddress* addr);

string GetXBeeID(XBeeAddress* addr) {
	char* s = new char[17];
	int i;
	for(i=0; i<8; i++) {
		sprintf(&s[i<<1], "%02x", addr->addr[i]);
	}

	string _s = string(s);
	delete s;
	return _s;
}

void SensorUpdate(XBeeAddress* addr) {
	Sensor* sensor = sensorMap[*addr];
	if(sensor==NULL) {
		AddSensor(addr);
		sensor = sensorMap[*addr];
	}
	
	sensor->lastPacketTime = time(NULL);
}

void AddSensor(XBeeAddress* addr) {
	if(sensorMap[*addr]!=NULL) {
		return;
	}
	
	Sensor* sensor = new Sensor;
	memcpy(&sensor->addr, addr, sizeof(XBeeAddress));

	printf("id=%s\n", GetXBeeID(addr).c_str());
	
	SensorDB db;

	// Add the network if necessary.
	db.AddNetwork(GetXBeeID(&receiver_addr));
	
	// Add the sensor to the network DB.
	db.AddSensor(GetXBeeID(&receiver_addr), GetXBeeID(addr));

	sensorMap[*addr] = sensor;
}

void* sensor_scanning_thread(void* p) {
	while(1) {
		map<XBeeAddress, Sensor*>::iterator itr;
		for(itr = sensorMap.begin(); itr != sensorMap.end(); itr++) {
			if(itr->second == NULL) {
				printf("Encountered NULL Sensor. %d\n", sensorMap.size());
				continue;
			}
			
			if(itr->second->lastPacketTime+90 < time(NULL) && itr->second->lastPacketTime) {
				cout<< "Sensor has gone more than 90 seconds without replying.\n";
			}
			
		}
		
		sleep(1);
	}
}

void SendPacket(SerialPort* port, XBeeAddress* address, Packet* packet) {
	XBAPI_Transmit(port, address, packet, sizeof(Packet));
}

void SendReceiverAddress(SerialPort* port, XBeeAddress* dest) {
	for(int i=0; i<sizeof(XBeeAddress); i++) {
		printf("%x%c", dest->addr[i], (i==sizeof(XBeeAddress)-1) ? '\n' : ' ');
	}

	Packet packet_buffer;
    packet_buffer.header.command = REQUEST_RECEIVER;
    packet_buffer.header.magic = 0xAA55;
    packet_buffer.header.revision = PROGRAM_REVISION;
    //packet_buffer.header.crc16 = CRC16_Generate((byte*)&packet_buffer, sizeof(Packet));
    // TODO:  Implement CRC16 generation.

    SendPacket(port, dest, &packet_buffer);
}

void hexdump(void* ptr, int len) {
	unsigned char* addr = (unsigned char*) ptr;
	int i;
	for(i=0; i<len; i++) {
		printf("%x%c", addr[i], (i%16) ? ' '  : '\n');
	}
}

XBeeAddress TransformTo8ByteAddress(XBeeAddress_7Bytes address_7bytes) {
	XBeeAddress address;

	// Do transformation.
	memset(&address, 0, sizeof(XBeeAddress));

	// Copy it over.  Maybe we should have some special code in case the sizes change in the future or something.  Oh well, it's not like it's gonna change very soon.
	memcpy(&address.addr[1], &address_7bytes, sizeof(XBeeAddress_7Bytes));

	return address;
}

#define ZEROC_INKELVIN 273.15

void HandlePacket(SerialPort* port, Frame* apiFrame) {
	printf("HELP US ALL!\n");

	Packet* packet = &apiFrame->rx.packet;
	hexdump(apiFrame, sizeof(Frame));

	switch(packet->header.command) {
		case REQUEST_RECEIVER: {
			printf("receiver request\n");
			XBeeAddress xbee_addr;
			xbee_addr = TransformTo8ByteAddress(apiFrame->rx.source_address); // Because of some weird design oversight on the xbee engineer's part, I have to such things as this.

			SendReceiverAddress(port, &xbee_addr);
			
			if(sensorMap[xbee_addr] == NULL) {
				AddSensor(&xbee_addr); // It should be adding here, but it's not.  It's waiting till the report.  FIXME
			}
			
			LogEntry entry;
			entry.sensorId = xbee_addr;
			entry.time = time(NULL);

			Logger_AddEntry(&entry, REQUEST_RECEIVER);
			SensorUpdate(&xbee_addr); // This will update the receiver struct and let the other thread know not to throw a fit.
		} break;

		case REPORT: {
			//hexdump(packet, 32);
			int resistance = packet->report.thermistorResistance;
			int res25C = packet->report.thermistorResistance25C;
			int probeBeta = packet->report.thermistorBeta;
			
			double probe0_temp = 1.0 / ((log((double)resistance / res25C) / probeBeta)+(1/(ZEROC_INKELVIN+25))) - ZEROC_INKELVIN;
			
			printf("probe0_temp=%f\n", probe0_temp);
			LogEntry entry;
			entry.resistance = packet->report.thermistorResistance;
			XBeeAddress address;

			// We get to do this because some guy thought it was a brilliant idea to use 7 byte addresses instead of 8 byte on the receive indicator.
			address = TransformTo8ByteAddress(apiFrame->rx.source_address);
			entry.sensorId = address;
			entry.time = time(NULL);
			entry.xbee_reset = apiFrame->rx.packet.report.xbee_reset;

			Logger_AddEntry(&entry, packet->header.command);
			
			if(sensorMap[address] == NULL) {
				AddSensor(&address);
			}
			
			SensorUpdate(&address);
			
			SensorDB db;
			db.AddReport(GetXBeeID(&address), time(NULL), probe0_temp);
		} break;
	}
}

extern unsigned swap_endian_32(unsigned n);

int main() {
	SerialPort* port = NULL;

	try {
		port = new SerialPort(9600);
	} catch(SerialPortException e) {
		printf("We did not find a valid port.\n");
		return 1;
	}

	printf("We are go\n");

	pthread_t thread;
	pthread_create(&thread, NULL, &sensor_scanning_thread, NULL);

	// TODO:  Add sensor config loading code here.

	// Get our Xbee ID.
	unsigned* buffer = new unsigned[16];
	XBAPI_Command(port, API_CMD_ATSL, &buffer[1], 1, 0);
	XBAPI_Command(port, API_CMD_ATSH, &buffer[0], 1, 0);
	buffer[0] = swap_endian_32(buffer[0]);
	buffer[1] = swap_endian_32(buffer[1]);
	
	memcpy(&receiver_addr, ((XBeeAddress*)buffer), sizeof(XBeeAddress));
	 
	while(1) {
		XBAPI_HandleFrame(port, 0);
		//printf("while()\n");
		//port->read(buffer, 4);
		//printf("%x %x %x %x ", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
