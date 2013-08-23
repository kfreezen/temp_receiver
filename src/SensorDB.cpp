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

using namespace std;

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

const char* __server = "localhost";

int processData(char* _buffer, int size, int nmemb, void* userPointer) {
	CURLRecvStruct* curlRecv = (CURLRecvStruct*) userPointer;

	// First we see if the structure needs to be initialized.
	if(curlRecv->buffer == NULL) {
		curlRecv->buffer = new char[(size*nmemb>256)?size*nmemb+256:256];
		curlRecv->length = 256;
		curlRecv->currentItr = 0;
	} else if(curlRecv->currentItr+size*nmemb > curlRecv->length) { // This transfer will go over the buffer's length, so increase buffer size.
		char* oldBuffer = curlRecv->buffer;
		int newLength = curlRecv->length<<1;
		if(newLength < curlRecv->currentItr+size*nmemb) {
			newLength = curlRecv->length+size*nmemb+1;
		}
		curlRecv->buffer = new char[newLength];
		curlRecv->length = newLength;

		memcpy(curlRecv->buffer, oldBuffer, curlRecv->currentItr);
		delete oldBuffer;
	}

	// Now let us copy the new stuff in buffer.
	memcpy(curlRecv->buffer+curlRecv->currentItr, _buffer, size*nmemb);

	return size*nmemb;
}

bool SensorDB::AddNetwork(std::string recv_id) {
	if(recv_id.length() != ID_LENGTH) {
		fprintf(stderr, "recv_id.length()=%d, should equal 16.", recv_id.length());
		throw SensorDBException("recv_id wrong length");
	}

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if(curl == NULL) {
		throw SensorDBException("curl==NULL");
	}

	string serverCallString = string(__server) + string("/networks.php?do=add");
	char* cPOST = new char[128];
	char* recv_id_encoded = curl_easy_escape(curl, recv_id.c_str(), recv_id.length());

	sprintf(cPOST, "recv_id=%s", recv_id_encoded);
	curl_free(recv_id_encoded);

	CURLRecvStruct data;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, processData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_URL, serverCallString.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void*)cPOST);
	if(curl_easy_perform(curl)) {
		fprintf(stderr, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
	} else {
		// Now we parse the string.
		printf("%s\n", data.buffer);
		if(!strcmp(data.buffer, "true")) {
			return true;
		} else if(!strcmp(data.buffer, "false")){
			return false;
		}
	}
}

bool SensorDB::AddSensor(std::string recv_id, std::string sensor_id) {
	printf("stub:  SensorDB::AddSensor(%s, %s);  TODO:  Finish.\n", recv_id.c_str(), sensor_id.c_str());
	
	// TODO:  Finish.
	
	return false;
}
