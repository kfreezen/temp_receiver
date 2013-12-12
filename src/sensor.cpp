#include "sensor.h"
#include "xbee.h"
#include "SensorDB.h"

#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

extern FILE* __stdout_log;
extern XBeeAddress receiver_addr; // TODO:  Get rid of this extern

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
	delete s;
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


void SensorUpdate(SensorId* id) {
	Sensor* sensor = sensorMap[*id];
	if(sensor==NULL) {
		AddSensor(id);
		sensor = sensorMap[*id];
	}

	sensor->lastPacketTime = time(NULL);
}

void AddSensor(SensorId* id) {
	if(sensorMap[*id]!=NULL) {
		return;
	}
	
	Sensor* sensor = new Sensor;
	memcpy(&sensor->id, id, sizeof(SensorId));

	//fprintf(__stdout_log, "id=%s\n", GetXBeeID(addr).c_str());
	
	SensorDB db;

	// Add the network if necessary.
	db.AddNetwork(GetXBeeID(&receiver_addr));
	
	// Add the sensor to the network DB.
	db.AddSensor(GetXBeeID(&receiver_addr), GetID(id));

	sensorMap[*id] = sensor;
}

map<XBeeAddress, SensorId> GetXBeeAddressToSensorIdMap() {
	return XBeeAddressToSensorIdMap;
}

map<SensorId, Sensor*> GetSensorMap() {
	return sensorMap;
}
