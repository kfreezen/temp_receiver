#ifndef UTIL_H
#define UTIL_H

#include <ctime>
#include <cstdio>

#include "globaldef.h"

struct timespec now();
struct timespec add_timespec(struct timespec a, struct timespec b);
struct timespec sub_timespec(struct timespec a, struct timespec b);
int compare_timespec(struct timespec a, struct timespec b);

// Gets the current time as a string.
// Uses threadsafe version of ctime
char* getCurrentTimeAsString();

// 
char* getTimeAsString(time_t timestamp);

int nsleep(uint64_t ns);

char* getErrorString();

void hexdump(void* ptr, int len);
void fhexdump(FILE* f, void* ptr, int len);

#endif
