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
			delete buffer;
		}
	}

	CURLBuffer() {
		buffer = NULL;
		length = 0;
		currentItr = 0;
	}

	char* buffer;
	int length;
	int currentItr;

	void init(int totalBytes);

	void extend(int totalBytes);
};

class SimpleCurl {
public:
	SimpleCurl();
	~SimpleCurl();

	CURLBuffer* post(string url, string data, int postType=POST_JSON);
	CURLBuffer* get(string url, string data);

	string escape(string toEscape);
	
private:
	static int writeFunc(char* buffer, int size, int nmemb, void* userPointer);
	CURL* curlHandle;
};
#endif
