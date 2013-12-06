#ifndef SENSOR_H
#define SENSOR_H

#include "xbee.h"
#include <map>

#include "globaldef.h"

using namespace std;

typedef struct {
	SensorId id;
	int lastPacketTime;
} Sensor;

typedef map<SensorId, Sensor*> SensorMap;

void AddSensor(SensorId* addr);

SensorMap GetSensorMap();

void SensorUpdate(SensorId* addr);

bool operator<(const SensorId& left, const SensorId& right);

#endif
