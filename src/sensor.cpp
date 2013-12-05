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

extern string GetXBeeID(SensorId* addr);
void SensorUpdate(SensorId* addr) {
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

	fprintf(__stdout_log, "id=%s\n", GetXBeeID(addr).c_str());
	
	SensorDB db;

	// Add the network if necessary.
	db.AddNetwork(GetXBeeID(&receiver_addr));
	
	// Add the sensor to the network DB.
	db.AddSensor(GetXBeeID(&receiver_addr), GetXBeeID(addr));

	sensorMap[*addr] = sensor;
}

map<XBeeAddress, SensorId> GetXBeeAddressToSensorIdMap() {
	return XBeeAddressToSensorIdMap;
}

map<XBeeAddress, Sensor*> GetSensorMap() {
	return sensorMap;
}
