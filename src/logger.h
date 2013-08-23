#ifndef LOGGER_H
#define LOGGER_H

#include <ctime>

#include "xbee.h"

// Note that this structure is subject to change.
typedef struct {
	XBeeAddress sensorId;
	time_t time;
	unsigned resistance;
	int xbee_reset;
} LogEntry;

// These functions will log the data to a mysql server. (Not really in this revision, this revision will just store it as plain text in a local text file)

// Function prototypes
void Logger_Init(const char* server_address);
void Logger_AddEntry(LogEntry* entry, int command);

// This function should have some kind of semi-fuzzy logic.
/// To search through all nodes, leave address NULL, otherwise point to legitimate 64-bit Xbee address.
LogEntry* Logger_FindEntry(XBeeAddress* address, time_t time);

LogEntry* Logger_GetEntry(int itr);

#endif
