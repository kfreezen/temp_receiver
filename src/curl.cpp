#include "curl.h"
#include <curl/curl.h>
#include <exception>
#include <cstdio>
#include <cstring>

#include "util.h"

//#define CURL_DEBUG 0

using namespace std;

int noWeb = 0;

void CURLBuffer::init(int totalBytes) {
	if(this->buffer != NULL) {
		delete this->buffer;
	}

	try {
		this->buffer = new char[(totalBytes>256)?totalBytes+256:256];
		this->capacity = (totalBytes>256) ? totalBytes + 256 : 256;

	} catch(bad_alloc& ba) {
		printf("CURLBuffer::init() -- bad alloc caught:  %s\n", ba.what());
		return;
	}

	memset(this->buffer, 0, (totalBytes>256) ? totalBytes+256:256);

}

void CURLBuffer::extend(int totalBytes) {
	char* oldBuffer = this->buffer;
	int newCapacity = this->capacity << 1; // this->capacity * 2
	if(newCapacity < this->length+totalBytes) {
		newCapacity = this->capacity + totalBytes + 1;
	}
	
	printf("a__");
	try {
		this->buffer = new char[newCapacity];
	} catch(std::bad_alloc& ba) {
		printf("Failed allocation\n");
	}

	printf("buffer=%p\n", this->buffer);

	this->capacity = newCapacity;
	memset(this->buffer, 0, newCapacity);

	memcpy(this->buffer, oldBuffer, this->length);
	delete[] oldBuffer;
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

int SimpleCurl::writeFileFunc(void* ptr, int size, int nmemb, FILE* stream) {
	return fwrite(ptr, size, nmemb, stream);
}

int SimpleCurl::writeFunc(char* buffer, int size, int nmemb, void* userPointer) {
	printf("starting _");
	CURLBuffer* buf = static_cast<CURLBuffer*>(userPointer);

	if(buf->buffer == NULL) {
		buf->init(size*nmemb);
	} else if(buf->length + size*nmemb > buf->capacity) {
		printf("extend");
		buf->extend(size*nmemb);
	}
	
	#ifdef CURL_DEBUG
	printf("memcpy %p, %p, %x, %x\n", buf->buffer, buffer, buf->capacity, size*nmemb);
	#endif

	memcpy(buf->buffer + buf->length, buffer, size*nmemb);

	buf->length += size * nmemb;
	
	printf("writeFunc\n");

	return size*nmemb;
}

string SimpleCurl::escape(string toEscape) {
	char* escapedString = curl_easy_escape(this->curlHandle, toEscape.c_str(), toEscape.capacity());
	string escapedStringRet = string(escapedString);
	curl_free(escapedString);
	return escapedStringRet;
}

#define CURL_CONNECT_TIMEOUT 10000

void SimpleCurl::download(string url, string fileName) {
	if(noWeb) {
		return;
	}
	
	string fullUrl = url;
	fullUrl += "?";
	//fullUrl += data;
	
	FILE* stream = fopen(fileName.c_str(), "w");

	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEFUNCTION, SimpleCurl::writeFileFunc);
	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEDATA, stream);
	curl_easy_setopt(this->curlHandle, CURLOPT_URL, fullUrl.c_str());
	curl_easy_setopt(this->curlHandle, CURLOPT_CONNECTTIMEOUT_MS, CURL_CONNECT_TIMEOUT);
	// TODO:  Fix if timeout turns out to be problem.
	
	//curl_easy_setopt(this->curlHandle, CURLOPT_POST, 1);
	//curl_easy_setopt(this->curlHandle, CURLOPT_POSTFIEDS, (void*) data.c_str());
	
	int err = curl_easy_perform(this->curlHandle);

	if(err) {
		printf("Something went wrong.  err=%d, %s, %d\n", err, __FILE__, __LINE__);
		
		curl_easy_reset(this->curlHandle);

		fclose(stream);
	} else {
		curl_easy_reset(this->curlHandle);
		
		fclose(stream);
	}
}

CURLBuffer* SimpleCurl::get(string url, string data) {
	if(noWeb) {
		return NULL;
	}
	
	CURLBuffer* buf = new CURLBuffer;
	string fullUrl = url;
	fullUrl += "?";
	fullUrl += data;

	#if CURL_DEBUG == 1
	curl_easy_setopt(this->curlHandle, CURLOPT_VERBOSE, 1);
	#endif

	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEFUNCTION, SimpleCurl::writeFunc);
	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(this->curlHandle, CURLOPT_URL, fullUrl.c_str());
	curl_easy_setopt(this->curlHandle, CURLOPT_CONNECTTIMEOUT_MS, CURL_CONNECT_TIMEOUT);
	// TODO:  Fix if timeout turns out to be problem.
	
	//curl_easy_setopt(this->curlHandle, CURLOPT_POST, 1);
	//curl_easy_setopt(this->curlHandle, CURLOPT_POSTFIEDS, (void*) data.c_str());
	
	int err = curl_easy_perform(this->curlHandle);

	if(err) {
		printf("Something went wrong.  err=%d, get %s\n", err, fullUrl.c_str());
		logInternetError(err, NULL, 0);

		curl_easy_reset(this->curlHandle);
		delete buf;
		return NULL;
	} else {
		curl_easy_reset(this->curlHandle);
		return buf;
	}
}

CURLBuffer* SimpleCurl::post(string url, string data, int postType) {
	if(noWeb) {
		return NULL;
	}

	CURLBuffer* buf = new CURLBuffer;

	char* postfields = new char[data.capacity()+1];
	strcpy(postfields, data.c_str());

	#if CURL_DEBUG == 1
	curl_easy_setopt(this->curlHandle, CURLOPT_VERBOSE, 1);
	#endif

	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEFUNCTION, SimpleCurl::writeFunc);
	curl_easy_setopt(this->curlHandle, CURLOPT_WRITEDATA, buf);
	curl_easy_setopt(this->curlHandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(this->curlHandle, CURLOPT_CONNECTTIMEOUT_MS, CURL_CONNECT_TIMEOUT);

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

	int err = curl_easy_perform(this->curlHandle);
	if(err) {
		printf("Something went wrong.  err=%d, post %s, args %s\n", err, url.c_str(), data.c_str());
		logInternetError(err, NULL, 0);

		curl_slist_free_all(headers);
		curl_easy_reset(this->curlHandle);
		
		delete[] postfields;
		delete buf;
		return NULL;
	} else {

		curl_slist_free_all(headers);
		curl_easy_reset(this->curlHandle);
		
		delete[] postfields;
		return buf;
	}
}

#define SINGLE_LINE_DATA_LENGTH 24
#define MAX_DATA_LENGTH 512
#define MAX_LOG_SIZE_MB 8

void logInternetError(int errorCode, const char* data, int length) {
	if(data == NULL) {
		length = 0;
	}

	FILE* errLog = fopen("internet-issues.log", "a+");
	int size = ftell(errLog);
	if(size >= (MAX_LOG_SIZE_MB*1024*1024)) {
		fclose(errLog);
		errLog = fopen("internet-issues.log", "w");
		
		char* timeStr = getCurrentTimeAsString();
		fprintf(errLog, "new log: %s\n", timeStr);
		delete[] timeStr;
	}

	
	char* timeStr = getCurrentTimeAsString();
	fprintf(errLog, "%s: ", timeStr);
	delete[] timeStr;

	if(errorCode) {
		fprintf(errLog, "err=%d ", errorCode);

	}

	if(length > 0) {
		fprintf(errLog, "data =%c", (length < SINGLE_LINE_DATA_LENGTH) ? ' ' : '\n');
		if(length > MAX_DATA_LENGTH) {
			length = MAX_DATA_LENGTH;
		}

		fwrite(data, sizeof(char), length, errLog);
	}

	fprintf(errLog, "\n");

	fclose(errLog);
}

// Our CURL library test suite.
// to find bugs, of course.
bool curlTest() {
	SimpleCurl* _dcurl = new SimpleCurl();

	CURLBuffer* buf = _dcurl->get("localhost:8089/fault/empty", "");

	if(buf) {
		delete buf;
	}

	CURLBuffer* buf2 = _dcurl->post("localhost:8089/fault/empty", "{\"arg0\": \"hi\"}");

	if(buf2) {
		delete buf2;
	}

	delete _dcurl;

	printf("Delete test passed\n");


	SimpleCurl curl;

	// First off, we do testing for an empty response.
	buf = curl.get("localhost:8089/fault/empty", "");
	
	buf2 = curl.post("localhost:8089/fault/empty", "{\"arg0\": \"hi\"}");

	if(buf) {
		delete buf;
	}

	if(buf2) {
		delete buf2;
	}

	// Now do the same on a nonexistant server.
	buf = curl.get("localhost:8090", "");

	buf2 = curl.post("localhost:8090", "{\"arg0\": \"hi\"}");

	if(buf) {
		delete buf;
	}

	if(buf2) {
		delete buf2;
	}
	
	return true;
}