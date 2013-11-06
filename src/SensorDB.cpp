/*
 * SensorDB.cpp
 *
 *  Created on: Aug 19, 2013
 *      Author: kent
 */

#include "SensorDB.h"
#include <curl/curl.h>
#include <exception>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sstream>
#include <map>

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

map<string, string> settings;
//const char* settings["server"] = "http://localhost";

void loadSettings(const char* file) {
	ifstream in;
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

	in.close();
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
	memcpy(curlRecv->buffer+curlRecv->currentItr, _buffer, size*nmemb);
	return size*nmemb;
}

bool SensorDB::AddNetwork(std::string net_id) {
	if(net_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "net_id.length()=%d, should equal 16.", net_id.length());
		throw SensorDBException("net_id wrong length");
	}

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if(curl == NULL) {
		throw SensorDBException("curl==NULL");
	}

	string serverCallString = string(settings["server"]) + string("/networks.php?do=add");
	char* cPOST = new char[128];
	char* net_id_encoded = curl_easy_escape(curl, net_id.c_str(), net_id.length());
	
	sprintf(cPOST, "net_id=%s", net_id_encoded);
	curl_free(net_id_encoded);
	
	CURLRecvStruct data;
	memset(&data, 0, sizeof(CURLRecvStruct));
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, processData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_URL, serverCallString.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)cPOST);
	
	bool retVal = false;
	
	if(curl_easy_perform(curl)) {
		fprintf(__stdout_log, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
	} else {
		// Now we parse the string.
		if(data.buffer==NULL) {
			fprintf(__stdout_log, "ERROR:  data.buffer is null but shouldn't be.\n");
			retVal = false;
		}
		
		if(!strcmp(data.buffer, "true")) {
			retVal = true;
		} else if(!strcmp(data.buffer, "false")) {
			retVal = false;
		}
	}
	
	delete cPOST;
	
	curl_easy_cleanup(curl);
	
	if(data.buffer != NULL) {
		delete data.buffer;
	}

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
	
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if(curl == NULL) {
		throw SensorDBException("curl==NULL");
	}
	
	string serverCallString = settings["server"] + string("/sensors.php?do=add");
	char* cPOST = NULL;
	try {
		cPOST = new char[128];
	} catch(bad_alloc& ba) {
		fprintf(__stdout_log, "bad_alloc caught %s\n", ba.what());
	}

	char* net_id_encoded = curl_easy_escape(curl, net_id.c_str(), net_id.length());
	char* sensor_id_encoded = curl_easy_escape(curl, sensor_id.c_str(), sensor_id.length());
	
	sprintf(cPOST, "net_id=%s&sensor_id=%s&recv_auth=%s", net_id_encoded, sensor_id_encoded, RECV_AUTH);
	curl_free(net_id_encoded);
	curl_free(sensor_id_encoded);
	
	CURLRecvStruct data;
	memset(&data, 0, sizeof(CURLRecvStruct));
	
	fprintf(__stdout_log, "%s:%s\n", serverCallString.c_str(), cPOST);
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, processData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_URL, serverCallString.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)cPOST);
	
	bool retVal = false;
	
	if(curl_easy_perform(curl)) {
		fprintf(__stdout_log, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
	} else {
		// Now we parse the string.
		if(data.buffer==NULL) {
			fprintf(__stdout_log, "ERROR:  data.buffer is null but shouldn't be.\n");
			retVal = false;
		}

		if(!strcmp(data.buffer, "true")) {
			retVal = true;
		} else if(!strcmp(data.buffer, "false")) {
			retVal = false;
		}
	}
	
	delete cPOST;
	
	// Clean up the receive structure
	if(data.buffer != NULL) {
		delete data.buffer;
	}

	curl_easy_cleanup(curl);
	
	return retVal;
}

#define TEMP_TYPE "temp"
#define PROBE_NUM 0

bool SensorDB::AddReport(std::string sensor_id, time_t timestamp, double probe0_value) {

	bool retVal;
	
	if(sensor_id.length() != ID_LENGTH) {
		fprintf(__stdout_log, "sensor_id.length()=%d, should equal %d.", sensor_id.length(), ID_LENGTH);
		throw SensorDBException("sensor_id wrong length");
	}
	
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if(curl == NULL) {
		throw SensorDBException("curl==NULL");
	}
	
	string serverCallString = settings["server"] + string("/report.php?do=add");
	char* cPOST = NULL;
	try {
		cPOST = new char[128];
	} catch(bad_alloc& ba) {
		fprintf(__stdout_log, "bad_alloc caught %s\n", ba.what());
	}

	char* sensor_id_encoded = curl_easy_escape(curl, sensor_id.c_str(), sensor_id.length());
	
	sprintf(cPOST, "timestamp=%lu&sensor_id=%s&probe0_value=%f&type=%s&probe_num=%d", timestamp, sensor_id_encoded, probe0_value, TEMP_TYPE, PROBE_NUM);
	curl_free(sensor_id_encoded);
	
	CURLRecvStruct data;
	memset(&data, 0, sizeof(CURLRecvStruct));
	
	//fprintf(__stdout_log, "%s:%s\n", serverCallString.c_str(), cPOST);
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, processData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_URL, serverCallString.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)cPOST);
	
	if(curl_easy_perform(curl)) {
		fprintf(__stdout_log, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
	} else {
		// Now we parse the string.
		if(data.buffer==NULL) {
			fprintf(__stdout_log, "ERROR:  data.buffer is null but shouldn't be.\n");
			retVal = false;
		}
		
		if(!strcmp(data.buffer, "true")) {
			retVal = true;
		} else if(!strcmp(data.buffer, "false")) {
			retVal = false;
		} else {
			retVal = false;
			fprintf(__stdout_log, "data:%s\n", data.buffer);
		}
	}
	
	delete cPOST;
	
	// Clear receive structure
	if(data.buffer != NULL) {
		delete data.buffer; // This delete and the other data.buffer 
		// delete will take care of one memory leak.
	}

	curl_easy_cleanup(curl);
	
	return retVal;
}
