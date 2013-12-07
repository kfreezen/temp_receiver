#include "tests.h"
#include "curl.h"

#include <cstring>

int CURL_ModuleTest();

int tests() {
	int fail = 0;
	fail |= CURL_ModuleTest();
	return fail;
}

int CURL_ModuleTest() {
	SimpleCurl c;
	CURLBuffer* b = c.get("http://192.168.1.251/api/test", "");
	printf("b->buffer = %s\n", b->buffer);
	if(strcmp(b->buffer, "test_success")) {
		return 1;
	}

	delete b;
	printf("deleted CURLBuffer* number 1\n");

	b = c.post("http://192.168.1.251/api/test", "{\"test\": 1}", POST_JSON);
	printf("b->buffer posted = %s\n", b->buffer);
	if(strcmp(b->buffer, "test_success")) {
		printf("b->buffer != test_success.  b->buffer = %p\n", b->buffer);
		return 1;
	}

	delete b;
	printf("deleted CURLBuffer* number 2\n");

	return 0;
}