#include "logger.h"
#include "util.h"
#include "ServerComm.h"

#include <fstream>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <errno.h>
#include <cstring>

using namespace std;

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
		printf("%u receiver request #%s\n", entry->time, /*GenerateXBeeAddressString(entry->sensorId),*/ ctime(&entry->time));
		break;
	}
	
	case REPORT: {
		printf("%u %d #%s\n", entry->time, entry->resistance, ctime(&entry->time));
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

void Logger_Print(int logType, time_t timestamp, const char* fmt, ...) {
	FILE* logFile = fopen("generic.log", "a+");
	FILE* _chosen_consoleout = NULL;
	switch(logType) {
		case INFO:
			_chosen_consoleout = stdout;
			break;
		case WARNING:
		case ERROR:
			_chosen_consoleout = stderr;
			break;
			
		default:
			_chosen_consoleout = stdout;
	}

	if(logFile == NULL) {
		fprintf(stderr, "logFile fopen() failed.  why=%s\n", strerror(errno));
		return;
	}

	if(ftell(logFile) >= MAX_GENERIC_LOG_SIZE_MB*1024*1024) {
		fclose(logFile);

		logFile = fopen("generic.log", "w");
		if(logFile == NULL) {
			fprintf(stderr, "logFile fopen(..., \"w\") failed.  why=%s\n", strerror(errno));
			return;
		}

		char* tstr = getCurrentTimeAsString();
		fprintf(logFile, "new log @ %s\n", tstr);
		delete[] tstr;
	}

	char* timeStr = getTimeAsString(timestamp);

	// Print out our timestamp
	fprintf(logFile, "%s: ", timeStr);
	fprintf(_chosen_consoleout, "%s: ", timeStr);
	delete[] timeStr;

	va_list args;

	va_start(args, fmt);
	vfprintf(logFile, fmt, args);
	va_end(args);

	va_start(args, fmt);
	vfprintf(_chosen_consoleout, fmt, args);
	va_end(args);

	if(ServerComm::comm != NULL && ServerComm::comm->isUsable()) {
		int len = strlen(fmt) + 256;
		char* str = new char[len];

		va_start(args, fmt);
		vsnprintf(str, len, fmt, args);
		va_end(args);
		
		RSCPContent* logContent = new RSCPContent;
		logContent->setRequest("LOG");
		logContent->addLine("message", str);

		char* tstamp = new char[32];
		snprintf(tstamp, 32, "%ld", time(NULL));
		logContent->addLine("timestamp", tstamp);
		delete[] tstamp;

		ServerComm::comm->sendContent(logContent);
	}

	fclose(logFile);
}