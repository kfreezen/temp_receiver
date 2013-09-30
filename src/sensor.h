#ifndef SENSOR_H
#define SENSOR_H

#include "xbee.h"
#include <map>

using namespace std;

typedef struct {
	XBeeAddress addr;
	int lastPacketTime;
} Sensor;

typedef map<XBeeAddress, Sensor*> SensorMap;

void AddSensor(XBeeAddress* addr);

SensorMap GetSensorMap();

void SensorUpdate(XBeeAddress* addr);

#endif
