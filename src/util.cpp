#include "util.h"

#include <ctime>
#include <cstring>

struct timespec now() {
	struct timespec nowTime;
	clock_gettime(CLOCK_REALTIME, &nowTime);
	return nowTime;
}

struct timespec add_timespec(struct timespec a, struct timespec b) {
	long ns = a.tv_nsec + b.tv_nsec;
	time_t s = a.tv_sec + b.tv_sec + (ns / 1000000000);
	ns = ns % 1000000000;
	struct timespec sp;
	sp.tv_sec = s;
	sp.tv_nsec = ns;
	return sp;
}

char* getCurrentTimeAsString() {
	char* time_buf = new char[128];

	time_t currentTime;
	currentTime = time(NULL);
	ctime_r(&currentTime, time_buf);
	
	int strlength = strlen(time_buf);

	if(time_buf[strlength-1] == '\n') {
		time_buf[strlength-1] = 0;
	}

	return time_buf;
}