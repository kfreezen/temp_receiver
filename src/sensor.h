#ifndef SENSOR_H
#define SENSOR_H

#include "xbee.h"
#include <map>

#include "globaldef.h"

using namespace std;

typedef struct {
	SensorId id;
	int lastPacketTime;
	bool isActive;
} Sensor;

typedef map<SensorId, Sensor*> SensorMap;

void AddSensor(SensorId* addr);

SensorMap GetSensorMap();

void SensorUpdate(SensorId* addr);

bool operator<(const SensorId& left, const SensorId& right);

string GetID(SensorId* id);
string GetXBeeID(XBeeAddress* addr);

FILE* sensorLogSetup();

#endif
