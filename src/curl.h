#ifndef __KENTS_CURL_H__
#define __KENTS_CURL_H__

#include <string>
#include <vector>
#include <curl/curl.h>

using namespace std;

#define POST_JSON 1
#define POST_FORMDATA 0

class CURLBuffer {
public:
	~CURLBuffer() {
		if(buffer != NULL) {
			delete[] buffer;
			buffer = NULL;
		}
	}

	CURLBuffer() {
		buffer = NULL;
		length = 0;
		capacity = 0;
	}

	char* buffer;
	int length;
	int capacity;

	void init(int totalBytes);

	void extend(int totalBytes);
};

class SimpleCurl {
public:
	SimpleCurl();
	~SimpleCurl();

	CURLBuffer* post(string url, string data, vector<string> headers, int postType=POST_JSON);
	CURLBuffer* get(string url, string data, vector<string> headers);

	void download(string url, string fileName);

	string escape(string toEscape);
	
	int getResponseCode() {
		return this->responseCode;
	}

private:
	void readCURLInfo();

	static int writeFunc(char* buffer, int size, int nmemb, void* userPointer);
	static int writeFileFunc(void* ptr, int size, int nmemb, FILE* stream);

	CURL* curlHandle;

	// This gets set by a call to .get or .post  The internal function readCURLInfo is used
	// to get this (via curl_easy_getinfo)
	int responseCode;

};

void logInternetError(int errCode, const char* data, int length);

bool curlTest();

#endif
