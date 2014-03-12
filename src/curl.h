#ifndef __KENTS_CURL_H__
#define __KENTS_CURL_H__

#include <string>
#include <curl/curl.h>

using std::string;

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

	CURLBuffer* post(string url, string data, int postType=POST_JSON);
	CURLBuffer* get(string url, string data);
	void download(string url, string fileName);

	string escape(string toEscape);
	
private:
	static int writeFunc(char* buffer, int size, int nmemb, void* userPointer);
	static int writeFileFunc(void* ptr, int size, int nmemb, FILE* stream);

	CURL* curlHandle;
};

void logInternetError(int errCode, const char* data, int length);

bool curlTest();

#endif
