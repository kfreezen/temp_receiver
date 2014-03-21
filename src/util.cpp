#include "util.h"
#include "logger.h"

#include <ctime>
#include <cstring>
#include <errno.h>

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


char* getTimeAsString(time_t timestamp) {
	char* time_buf = new char[128];

	ctime_r(&timestamp, time_buf);
	
	int strlength = strlen(time_buf);

	if(time_buf[strlength-1] == '\n') {
		time_buf[strlength-1] = 0;
	}

	return time_buf;
}

char* getCurrentTimeAsString() {
	return getTimeAsString(time(NULL));
}

int nsleep(uint64_t ns) {
	struct timespec _sleep_time, remaining;

	_sleep_time.tv_nsec = ns % 1000000000L;
	_sleep_time.tv_sec = ns / 1000000000L;

	if(nanosleep(&_sleep_time, &remaining) == -1) {
		if(errno == EINTR) {
			while(1) {
				if(nanosleep(&remaining, &remaining) == -1) {
					if(errno != EINTR) {
						return -1;
					}
				}
			}
		} else {
			return -1;
		}
	}
}

char* getErrorString() {
	const int ERRSTR_BUFSIZE = 256;

	char* errstrbuf = new char[ERRSTR_BUFSIZE];
	int err = errno;

	if(strerror_r(errno, errstrbuf, ERRSTR_BUFSIZE) != 0) {
		snprintf(errstrbuf, ERRSTR_BUFSIZE, "%d", err);
	}

	return errstrbuf;
}