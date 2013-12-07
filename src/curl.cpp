#include "curl.h"
#include <curl/curl.h>
#include <exception>
#include <cstdio>
#include <cstring>

using namespace std;

extern FILE* __stdout_log;

void CURLBuffer::init(int totalBytes) {
	if(this->buffer != NULL) {
		delete this->buffer;
	}

	try {
		this->buffer = new char[(totalBytes>256)?totalBytes+256:256];
	} catch(bad_alloc& ba) {
		fprintf(__stdout_log, "CURLBuffer::init() -- bad alloc caught:  %s\n", ba.what());
	}
}

void CURLBuffer::extend(int totalBytes) {
	char* oldBuffer = this->buffer;
	int newLength = this->length << 1; // this->length * 2
	if(newLength < this->currentItr+totalBytes) {
		newLength = this->length + totalBytes + 1;
	}

	this->buffer = new char[newLength];
	this->length = newLength;
	memset(this->buffer, 0, newLength);

	memcpy(this->buffer, oldBuffer, this->currentItr);
	delete oldBuffer;
}

SimpleCurl::SimpleCurl() {
	curl_global_init(CURL_GLOBAL_ALL);
	this->curlHandle = curl_easy_init();
}

SimpleCurl::~SimpleCurl() {
	if(this->curlHandle != NULL) {
		curl_easy_cleanup(this->curlHandle);
		this->curlHandle = NULL;
	}
}

int SimpleCurl::writeFunc(char* buffer, int size, int nmemb, void* userPointer) {
	CURLBuffer* buf = (CURLBuffer*) userPointer;

	if(buf->buffer == NULL) {
		buf->init(size*nmemb);
		try {
			buf->buffer = new char[(size*nmemb>256)?size*nmemb+256:256];
		} catch(bad_alloc& ba) {
			fprintf(__stdout_log, "bad alloc caught:  %s\n", ba.what());
		}
	} else if(buf->currentItr + size*nmemb > buf->length) {
		buf->extend(size*nmemb);
	}

	memcpy(buf->buffer + buf->currentItr, buffer, size*nmemb);

	buf->currentItr += size * nmemb;

	return size*nmemb;
}

CURLBuffer* SimpleCurl::get(string url, string data) {
	CURLBuffer* buf = new CURLBuffer;
	string fullUrl = url;
	fullUrl += "?";
	fullUrl += data;

	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEFUNCTION, SimpleCurl::writeFunc);
	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(this->curlHandle, CURLOPT_URL, fullUrl.c_str());
	//curl_easy_setopt(this->curlHandle, CURLOPT_POST, 1);
	//curl_easy_setopt(this->curlHandle, CURLOPT_POSTFIEDS, (void*) data.c_str());

	bool retVal = false;

	if(curl_easy_perform(this->curlHandle)) {
		fprintf(__stdout_log, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
		
		curl_easy_reset(this->curlHandle);
		delete buf;
		return NULL;
	} else {
		curl_easy_reset(this->curlHandle);
		return buf;
	}
}

CURLBuffer* SimpleCurl::post(string url, string data, int postType = POST_JSON) {
	CURLBuffer* buf = new CURLBuffer;

	char* postfields = new char[data.length()+1];
	strcpy(postfields, data.c_str());

	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEFUNCTION, SimpleCurl::writeFunc);
	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(this->curlHandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(this->curlHandle, CURLOPT_POST, 1);
	curl_easy_setopt(this->curlHandle, CURLOPT_POSTFIELDS, (void*) postfields);

	struct curl_slist* headers = NULL;
	switch(postType) {
		case POST_FORMDATA:
			headers = curl_slist_append(headers, "Content-type: application/x-www-form-urlencoded");
			break;
		case POST_JSON:
			headers = curl_slist_append(headers, "Content-type: application/json");
			break;
	}

	curl_easy_setopt(this->curlHandle, CURLOPT_HTTPHEADER, headers);

	bool retVal = false;

	if(curl_easy_perform(this->curlHandle)) {
		fprintf(__stdout_log, "Something went wrong.  %s, %d\n", __FILE__, __LINE__);
		
		curl_slist_free_all(headers);
		curl_easy_reset(this->curlHandle);
		
		delete postfields;
		delete buf;
		return NULL;
	} else {
		fprintf(__stdout_log, "Something went right.\n");
		
		curl_slist_free_all(headers);
		curl_easy_reset(this->curlHandle);
		
		delete postfields;
		return buf;
	}
}