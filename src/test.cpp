#include "test.h"
#include "curl.h"
#include "ServerComm.h"

bool doTests() {
	curlTest();
	ServerCommTests();
}