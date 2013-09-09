#include "logger.h"

#include <fstream>
#include <string>
#include <cstdio>

using namespace std;

extern FILE* __stdout_log;

void Logger_Init(const char* server_address) {
	/*
		For now we leave this blank.  Later on, the mysql code will need
	to check for a valid mysql database here, and if necessary, initialize properly.
	*/
}

void Logger_AddEntry(LogEntry* entry, int command) {
	// For now we just open up a text file and write to it.
	FILE* out;
	out = fopen("log.txt", "a");
	
	switch(command) {
	case REQUEST_RECEIVER:  {
		fprintf(__stdout_log, "%u receiver request #%s\n", entry->time, /*GenerateXBeeAddressString(entry->sensorId),*/ ctime(&entry->time));
		break;
	}
	
	case REPORT: {
		fprintf(__stdout_log, "%u %d #%s\n", entry->time, entry->resistance, ctime(&entry->time));
		break;
	}
	
	}
	
	fclose(out);
}


/****************************************************************/
/* Truth be told we probably don't need Logger_FindEntry(...)	*/
/* and Logger_GetEntry(...) for a receiver such as this.		*/
/****************************************************************/
// This function should have some kind of semi-fuzzy logic.
/// To search through all nodes, leave address NULL, otherwise point to legitimate 64-bit Xbee address.
LogEntry* Logger_FindEntry(XBeeAddress* address, time_t time) {
	// TODO:  Implement.
}

LogEntry* Logger_GetEntry(int itr) {
	// TODO:  Implement.
}
