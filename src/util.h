#ifndef UTIL_H
#define UTIL_H

#include <ctime>

#include "globaldef.h"

struct timespec now();
struct timespec add_timespec(struct timespec a, struct timespec b);

// Gets the current time as a string.
// Uses threadsafe version of ctime
char* getCurrentTimeAsString();

// 
char* getTimeAsString(time_t timestamp);

int nsleep(uint64_t ns);

char* getErrorString();

#endif
