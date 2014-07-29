/*
 * SensorDB.cpp
 *
 *  Created on: Aug 19, 2013
 *      Author: kent
 */

#include "SensorDB.h"
#include "settings.h"
#include "packets.h"
#include "util.h"

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
#include <vector>

#include "cJSON/cJSON.h"

typedef struct {
	std::string sensor_id;
	time_t timestamp;
	double* probeValue;
	double batteryLevel;
} Report;

#define RECV_AUTH "QmykRDNrEMfSJuLTSgzTWZyu"

using namespace std;

void logUnexpectedData(CURLBuffer* buf) {
	char* tstr = getCurrentTimeAsString();
	printf("data not as expected.  See log[%s] for details.\n", tstr);
	delete[] tstr;

	logInternetError(0, buf->buffer, buf->length);
}

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
	printf("You should use Settings::load instead.\n");

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
		printf("curlRecv == null.  ERROR\n");
		return 0;
	}
	
	#ifdef DEBUG
	printf("processDataStart %p,%d,%d,%p\nbuffer=%p,length=%d,currentItr=%d\n", _buffer, size, nmemb, userPointer, curlRecv->buffer, curlRecv->length, curlRecv->currentItr);
	#endif
	
	// First we see if the structure needs to be initialized.
	if(curlRecv->buffer == NULL) {
		try {
			curlRecv->buffer = new char[(size*nmemb>256)?size*nmemb+256:256];
		} catch(bad_alloc& ba) {
			printf("bad alloc caught:  %s\n", ba.what());
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

// Returns number of retries left.  non-zero (truthy) indicates success.  zero (falsy) indicates failure.
bool SensorDB::AddReceiver(std::string receiver_id) {
	SimpleCurl curl;

	string callString = Settings::get("server") + string("/api/receivers");
	cJSON* data;

	data = cJSON_CreateObject();
	cJSON_AddNumberToObject(report, "receiver_id", strtoul(receiver_id.c_str(), NULL, 16));

	cPOST = cJSON_Print(data);
	cJSON_Delete(data); // We're done with reportsData, so we need to delete it.

	vector<string> postHeaders;

	string serverCallString = Settings::get("server") + string("/api/reports");
	postHeaders.push_back("Accept-Version: 0.1");

	int retries = 3;
	CURLBuffer* buf = NULL;
	while(retries--) {
		if(buf != NULL) {
			delete buf;
			buf = NULL;
		}

		buf = curl.post(serverCallString, string(cPOST), postHeaders, POST_JSON);

		if(buf == NULL) {
			// Something failed.
			continue;
		} else if(curl.getResponseCode() != 200) {
			printf("CURL error:  Response code = %d, buffer = \"%s\"\n", curl.getResponseCode(), buf->buffer);
		} else {
			break;
		}
	}

	free(cPOST);

	return retries;
}

bool SensorDB::AddSensor(std::string net_id, std::string sensor_id) {
	/*#ifdef DEBUG
	printf("AddReportRun()\n");
	#endif
	
	if(net_id.length() != ID_LENGTH) {
		printf("net_id.length()=%d, should equal %d.", net_id.length(), ID_LENGTH);
		throw SensorDBException("net_id wrong length");
	}
	
	if(sensor_id.length() != ID_LENGTH) {
		printf("sensor_id.length()=%d, should equal %d.", sensor_id.length(), ID_LENGTH);
		throw SensorDBException("sensor_id wrong length");
	}
	
	SimpleCurl curl;
	
	string serverCallString = Settings::get("server") + string("/api/sensor/add");
	char* cPOST = NULL;
	
	try {
		cPOST = new char[256];
	} catch(bad_alloc& ba) {
		printf("bad_alloc caught %s\n", ba.what());
	}
	
	string encodedNetworkId = curl.escape(net_id);
	string encodedSensorId = curl.escape(sensor_id);

	sprintf(cPOST, "{\"sensor_id\": \"%s\", \"recv_auth\": \"%s\"}", encodedSensorId.c_str(), RECV_AUTH);

	printf("%s:%s\n", serverCallString.c_str(), cPOST);
	
	CURLBuffer* data = curl.post(serverCallString, string(cPOST), POST_JSON);
	if(data == NULL) {
		return false;
	}

	bool retVal;

	if(string(data->buffer) == "true") {
		retVal = true;
	} else if(string(data->buffer) == "false") {
		retVal = false;
	} else {
		retVal = false;

		logUnexpectedData(data);
	}

	delete[] cPOST;
	delete data;

	return retVal;*/
}

#define TEMP_TYPE "temperature"
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
		printf("sensor_id.length()=%d, should equal %d.", sensor_id.length(), ID_LENGTH);
		throw SensorDBException("sensor_id wrong length");
	}
	
	//curl_global_init(CURL_GLOBAL_ALL);
	SimpleCurl curl;
	
	char* cPOST = NULL;
	
	// Generate our reports structure for the web service.
	cJSON* reportsData, *reports, *report, *probe_data;
	reportsData = cJSON_CreateObject();

	cJSON_AddItemToObject(reportsData, "reports", reports = cJSON_CreateArray());

	cJSON_AddItemToArray(reports, report = cJSON_CreateObject());

	cJSON_AddNumberToObject(report, "sensor_id", strtoul(sensor_id.c_str(), NULL, 16));
	cJSON_AddNumberToObject(report, "date_logged", timestamp);
	cJSON_AddNumberToObject(report, "batt_level", batteryLevel);

	// Now, create the array of probes.
	cJSON_AddItemToObject(report, "probe_data", probe_data = cJSON_CreateArray());

	for(int i = 0; i < NUM_PROBES; i++) {
		cJSON* probe;
		
		// (overloaded isnan(double&) is ambigious) compiler error gets thrown unless we
		// un-ambiguate it with ::
		if(::isnan(probeValues[i])) {
			// Doesn't pay to process NaN, partly because JSON can't handle it, and
			// partly because NaN is indicative of an invalid probe.  Skip this entry.
			continue;
		}

		cJSON_AddItemToArray(probe_data, probe = cJSON_CreateObject());
		cJSON_AddNumberToObject(probe, "probe", i);
		
		cJSON_AddNumberToObject(probe, "value", probeValues[i]);
		
		cJSON_AddItemToObject(probe, "type", cJSON_CreateString(TEMP_TYPE));
	}

	cPOST = cJSON_Print(reportsData);
	cJSON_Delete(reportsData); // We're done with reportsData, so we need to delete it.

	vector<string> postHeaders;

	string serverCallString = Settings::get("server") + string("/api/reports");
	postHeaders.push_back("Accept-Version: 0.1");

	int retries = 3;
	CURLBuffer* buf = NULL;
	while(retries--) {
		if(buf != NULL) {
			delete buf;
			buf = NULL;
		}

		buf = curl.post(serverCallString, string(cPOST), postHeaders, POST_JSON);

		// buf == NULL indicates some sort of connection failure.
		if(buf == NULL) {
			continue;
		} else if(curl.getResponseCode() != 201) {
			// CURL response code should be 201

			printf("CURL error:  POST /api/reports response code = %d\nBuffer = \"%s\"\n", curl.getResponseCode(), buf->buffer);
			continue;
		} else {
			// Success.
			break;
		}
	}

	if(buf == NULL) {
		// We should add this to a list of failed reports.
		retVal = false;
	} else {
		if(buf->buffer == NULL) {
			printf("buf->buffer == NULL\n");
			retVal = false;
		} else {

			// Parse JSON, just in case there's some error info on the log.
			cJSON* json = cJSON_Parse(buf->buffer);

			if(!json) {
				printf("Failed JSON parse, raw response = \"%s\"\n", buf->buffer);
			} else {
				cJSON* errors = cJSON_GetObjectItem(json, "errors");
				if(!errors) {
					retVal = true;
				} else {
					// Print errors to log.
					int errorArraySize = cJSON_GetArraySize(errors);
					for(int i = 0; i < errorArraySize; i++) {
						cJSON* error = cJSON_GetArrayItem(errors, i);

						cJSON* errNumber = cJSON_GetObjectItem(error, "errno");
						cJSON* errDesc = cJSON_GetObjectItem(error, "errdesc");

						// These two ifs are to make sure that error number and error description
						// are the right types before we try to print them.
						if(errNumber->type == cJSON_Number) {
							printf("errno %d: ", errNumber->valueint);
						}

						if(errDesc->type == cJSON_String) {
							printf("desc \"%s\"\n", errDesc->valuestring);
						}
					}

					if(curl.getResponseCode() != 201) {
						// Did not return a 201 Created.  We should store this in a list of failed reports.
						// TODO
						retVal = false;
					} else {
						// Assume that everything was hunky-dory.
						retVal = true;
					}
				}

				if(curl.getResponseCode() != 201) {
					// Did not return a 201 Created.  We should store this in a list of failed reports.
					// TODO
					retVal = false;
				} else {
					// Assume that everything was hunky-dory.
					retVal = true;
				}

				cJSON_Delete(json);
			}
		}

		delete buf;
	}

	free(cPOST);

	return retVal;
}
