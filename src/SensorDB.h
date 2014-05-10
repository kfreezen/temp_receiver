/*
 * SensorDB.h
 *
 *  Created on: Aug 19, 2013
 *      Author: kent
 */

#ifndef SENSORDB_H_
#define SENSORDB_H_

#include <string>

void loadSettings(const char* file);

class SensorDB {
public:
	SensorDB();
	virtual ~SensorDB();
	
	/**
	 *   bool AddSensor(std::string net_id, std::string sensor_id):  Checks for a sensor and adds it to a network if necessary.
	 *   net_id:  This is the 16-character ID of the receiver's xbee, used to identify the network.
	 *   sensor_id:  This is the 16-character ID of the sensor's xbee, used to identify the sensor.
	 *   
	 *   This function checks to see if the sensor is on any network.  If it is, the function then checks to see if the sensor is
	 *   on the given network.  If the sensor is not on the given network, but does exist in the DB, it's network ID is altered to point to
	 *   the given network.  If the sensor does not exist in the DB, it is added to the given network.
	 *   
	 *   Returns:  true if the sensor existed in the DB, and false if the sensor entry had to be created.
	 *   Throws an exception if the arguments are invalid.
	 */
	 bool AddSensor(std::string net_id, std::string sensor_id);
	 
	 /**
	  *  bool AddReport(std::string sensor_id, time_t time, double probe0_temp):  Adds a report for a specified time.
	  *
	  */
	 bool AddReport(std::string sensor_id, time_t timestamp, double probe0_temp);
	 bool AddReport(std::string sensor_id, time_t timestamp, double* probeValues, double batteryLevel);

};

class SensorDBException {
public:
	SensorDBException(std::string whatWentWrong) {
		this->whatWentWrong = whatWentWrong;
	}

	std::string what() {
		return this->whatWentWrong;
	}

private:
	std::string whatWentWrong;
};
#endif /* SENSORDB_H_ */
