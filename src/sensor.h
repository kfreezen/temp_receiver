#ifndef SENSOR_H
#define SENSOR_H

#include "xbee.h"
#include <map>

#include "globaldef.h"

using namespace std;

typedef union SensorId {
	byte id[8];
	#ifdef __GNUC__
	uint64 uId;
	#endif
} SensorId;

typedef struct {
	SensorId addr;
	int lastPacketTime;
} Sensor;

typedef map<SensorId, Sensor*> SensorMap;

void AddSensor(XBeeAddress* addr);

SensorMap GetSensorMap();

void SensorUpdate(XBeeAddress* addr);

#endif
