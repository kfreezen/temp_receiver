#include "sensor.h"
#include "xbee.h"
#include "SensorDB.h"
#include "util.h"

#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

extern XBeeAddress receiver_addr; // TODO:  Get rid of this extern
extern string receiverId;

using namespace std;

map<XBeeAddress, SensorId> XBeeAddressToSensorIdMap;
map<SensorId, Sensor*> sensorMap;

bool operator<(const SensorId& left, const SensorId& right) {
	return left.uId < right.uId;
}

string GetID(SensorId* id) {
	char* s = new char[17];
	int i;
	for(i=0; i<8; i++) {
		sprintf(&s[i<<1], "%02x", id->id[i]);
	}

	string _s = string(s);
	delete[] s;
	return _s;
}

string GetXBeeID(XBeeAddress* addr) {
	/*char* s = new char[17];
	int i;
	for(i=0; i<8; i++) {
		sprintf(&s[i<<1], "%02x", addr->addr[i]);
	}

	string _s = string(s);
	delete s;
	return _s;*/
	return GetID((SensorId*) addr);
}

extern uint64 swap_endian_64(uint64 n);

void SensorUpdate(SensorId* id) {
	Sensor* sensor = sensorMap[*id];
	if(sensor==NULL) {
		AddSensor(id);
		sensor = sensorMap[*id];
	}

	sensor->lastPacketTime = time(NULL);
	if(!sensor->isActive) {
		sensor->isActive = true;

		FILE* sensorLog = sensorLogSetup();
		char* tstr = getCurrentTimeAsString();
		fprintf(sensorLog, "%s:  sensorId = %lx, sensor now active.\n", tstr, swap_endian_64(id->uId));
		delete[] tstr;

		fclose(sensorLog);
	}
}

void AddSensor(SensorId* id) {
	if(sensorMap[*id]!=NULL) {
		return;
	}

	Sensor* sensor = new Sensor;
	
	memset(sensor, 0, sizeof(Sensor));
	
	memcpy(&sensor->id, id, sizeof(SensorId));

	//printf("id=%s\n", GetXBeeID(addr).c_str());
	
	SensorDB db;

	// Add the network if necessary.
	db.AddNetwork(receiverId);
	
	// Add the sensor to the network DB.
	db.AddSensor(receiverId, GetID(id));

	sensorMap[*id] = sensor;
}

map<XBeeAddress, SensorId> GetXBeeAddressToSensorIdMap() {
	return XBeeAddressToSensorIdMap;
}

map<SensorId, Sensor*> GetSensorMap() {
	return sensorMap;
}

#define MAX_SENSOR_LOG_SIZE_MB 4

FILE* sensorLogSetup() {
	FILE* sensorLog = fopen("sensors.log", "a+");
	if(ftell(sensorLog) >= MAX_SENSOR_LOG_SIZE_MB*1024*1024) {
		fclose(sensorLog);
		sensorLog = fopen("sensors.log", "w");

		char* tstr = getCurrentTimeAsString();
		fprintf(sensorLog, "new log @ %s\n", tstr);
		delete[] tstr;
	}

	return sensorLog;
}