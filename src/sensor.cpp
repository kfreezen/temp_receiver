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

extern string GetXBeeID(XBeeAddress* addr);
extern string GetID(SensorId* id);
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
