/*
 * SensorDB.cpp
 *
 *  Created on: Aug 19, 2013
 *      Author: kent
 */

#include "SensorDB.h"
#include "settings.h"
#include "packets.h"

#include "curl.h"
#include <curl/curl.h>
#include <exception>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <cmath>

#define RECV_AUTH "QmykRDNrEMfSJuLTSgzTWZyu"

using namespace std;

extern FILE* __stdout_log;

SensorDB::SensorDB() {
	// TODO Auto-generated constructor stub

}

SensorDB::~SensorDB() {
	// TODO Auto-generated destructor stub
}

#define ID_LENGTH 16

typedef struct {
	int length;
	char* buffer;
	int currentItr;
} CURLRecvStruct;

//map<string, string> settings;
//const char* settings["server"] = "http://localhost";

#pragma gcc poison loadSettings
void loadSettings(const char* file) {
	Settings::load(string(file));
	fprintf(__stdout_log, "You should use Settings::load instead.\n");

	/*ifstream in;
	in.open(file);

	while(!in.eof()) {
		string str;
		getline(in, str);
		stringstream ss;
		ss.str(str);
		
		string key, value;
		ss>> key >> value;
		settings[key] = value;
	}

	in.close();*/
}

int processData(char* _buffer, int size, int nmemb, void* userPointer) {
	CURLRecvStruct* curlRecv = (CURLRecvStruct*) userPointer;

	if(curlRecv == NULL) {
		fprintf(__stdout_log, "curlRecv == null.  ERROR\n");
		return 0;
	}
	
	#ifdef DEBUG
	fprintf(__stdout_log, "processDataStart %p,%d,%d,%p\nbuffer=%p,length=%d,currentItr=%d\n", _buffer, size, nmemb, userPointer, curlRecv->buffer, curlRecv->length, curlRecv->currentItr);
	#endif
	
	// First we see if the structure needs to be initialized.
	if(curlRecv->buffer == NULL) {
		try {
			curlRecv->buffer = new char[(size*nmemb>256)?size*nmemb+256:256];
		} catch(bad_alloc& ba) {
			fprintf(__stdout_log, "bad alloc caught:  %s\n", ba.what());
		}

		curlRecv->length = (size*nmemb>256)?size*nmemb+256:256;
		memset(curlRecv->buffer, 0, curlRecv->length);
		curlRecv->currentItr = 0;
	} else if(curlRecv->currentItr+size*nmemb > curlRecv->length) { // This transfer will go over the buffer's length, so increase buffer size.
		char* oldBuffer = curlRecv->buffer;
		int newLength = curlRecv->length<<1;
		if(newLength < curlRecv->currentItr+size*nmemb) {
			newLength = curlRecv->length+size*nmemb+1;
		}
		curlRecv->buffer = new char[newLength];
		curlRecv->length = newLength;
		memset(curlRecv->buffer, 0, newLength);
		
		memcpy(curlRecv->buffer, oldBuffer, curlRecv->currentItr);
		delete oldBuffer;
	}
	
	// Now let us copy the new stuff in buffer.
	memcpy(curlRecv->buffer + curlRecv->currentItr, _buffer, size*nmemb);

	// This was added 12/5/13, I'm surpised there wasn't ever a visible bug here.
	curlRecv->currentItr += size*nmemb;

	return size*nmemb;
}

bool SensorDB::AddNetwork(std::string net_id) {
	if(net_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "net_id.length()=%d, should equal 16.", net_id.length());
		throw SensorDBException("net_id wrong length");
	}

	SimpleCurl curl;

	string serverCallString = Settings::get("server") + string("/api/network/add");
	char* cPOST = new char[128];
	
	string encodedNetworkId = curl.escape(net_id);

	sprintf(cPOST, "{\"network_id\": \"%s\"}", encodedNetworkId.c_str());
	
	CURLBuffer* data = curl.post(serverCallString, string(cPOST));
	if(data == NULL) {
		return false;
	}

	bool retVal = false;

	if(string(data->buffer) == "true") {
		retVal = true;
	} else if(string(data->buffer) == "false") {
		retVal = false;
	}
	
	delete cPOST;
	delete data;

	return retVal;
}

bool SensorDB::AddSensor(std::string net_id, std::string sensor_id) {
	#ifdef DEBUG
	fprintf(__stdout_log, "AddReportRun()\n");
	#endif
	
	if(net_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "net_id.length()=%d, should equal %d.", net_id.length(), ID_LENGTH);
		throw SensorDBException("net_id wrong length");
	}
	
	if(sensor_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "sensor_id.length()=%d, should equal %d.", sensor_id.length(), ID_LENGTH);
		throw SensorDBException("sensor_id wrong length");
	}
	
	SimpleCurl curl;
	
	string serverCallString = Settings::get("server") + string("/api/sensor/add");
	char* cPOST = NULL;
	
	try {
		cPOST = new char[256];
	} catch(bad_alloc& ba) {
		fprintf(__stdout_log, "bad_alloc caught %s\n", ba.what());
	}
	
	string encodedNetworkId = curl.escape(net_id);
	string encodedSensorId = curl.escape(sensor_id);

	sprintf(cPOST, "{\"network_id\": \"%s\", \"sensor_id\": \"%s\", \"recv_auth\": \"%s\"}", encodedNetworkId.c_str(), encodedSensorId.c_str(), RECV_AUTH);

	fprintf(__stdout_log, "%s:%s\n", serverCallString.c_str(), cPOST);
	
	CURLBuffer* data = curl.post(serverCallString, string(cPOST), POST_JSON);
	if(data == NULL) {
		return false;
	}

	bool retVal;

	if(string(data->buffer) == "true") {
		retVal = true;
	} else if(string(data->buffer) == "false") {
		retVal = false;
	}

	delete cPOST;
	
	return retVal;
}

#define TEMP_TYPE "temp"
#define PROBE_NUM 0

bool SensorDB::AddReport(std::string sensor_id, time_t timestamp, double probe0_value) {
	double vals[NUM_PROBES];
	vals[0] = probe0_value;
	int i;
	for(i = 1; i < NUM_PROBES; i++) {
		vals[i] = nan("");
	}

	// battery level is set to be 0.0, so that the web server knows not
	// to add this to a battery log.
	return SensorDB::AddReport(sensor_id, timestamp, vals, 0.0);
}

bool SensorDB::AddReport(std::string sensor_id, time_t timestamp, double* probeValues, double batteryLevel) {

	bool retVal;
	
	if(sensor_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "sensor_id.length()=%d, should equal %d.", sensor_id.length(), ID_LENGTH);
		throw SensorDBException("sensor_id wrong length");
	}
	
	//curl_global_init(CURL_GLOBAL_ALL);
	SimpleCurl curl;
	
	string serverCallString = Settings::get("server") + string("/api/reports/add");
	
	char* cPOST = NULL;
	try {
		cPOST = new char[512];
	} catch(bad_alloc& ba) {
		fprintf(__stdout_log, "bad_alloc caught %s\n", ba.what());
	}
	
	string sensor_id_encoded = curl.escape(sensor_id);
	
	// This JSON templating is bound to lead to countless problems.
	// We need to invest in a JSON encoder for C++.
	string probeValueStrings[NUM_PROBES];
	stringstream ss;
	int i;
	for(i = 0; i < NUM_PROBES; i++) {
		ss.str("");
		ss << probeValues[i];
		probeValueStrings[i] = ss.str();
	}

	sprintf(cPOST, "{ \
			\"timestamp\": %lu, \
			\"sensor_id\": \"%s\", \
			\"probe_values\": \
				[ \
					{\"num\": 0, \"val\": \"%s\", \"type\": \"%s\"}, \
			 		{\"num\": 1, \"val\": \"%s\", \"type\": \"%s\"}, \
			 		{\"num\": 2, \"val\": \"%s\", \"type\": \"%s\"}], \
			\"batt_level\": %f \
			}", timestamp, sensor_id_encoded.c_str(), probeValueStrings[0].c_str(),
			TEMP_TYPE, probeValueStrings[1].c_str(), TEMP_TYPE, probeValueStrings[2].c_str(), TEMP_TYPE,
			batteryLevel
		);

	CURLBuffer* buf = curl.post(serverCallString, string(cPOST), POST_JSON);
	if(buf == NULL) {
		return false;
	}
	
	if(!strcmp(buf->buffer, "true")) {
		retVal = true;
	} else if(!strcmp(buf->buffer, "false")) {
		retVal = false;
	} else {
		retVal = false;
		fprintf(__stdout_log, "data:%s\n", buf->buffer);
	}
	
	delete cPOST;
	delete buf;

	return retVal;
}
