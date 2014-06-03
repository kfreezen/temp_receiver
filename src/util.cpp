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

struct timespec sub_timespec(struct timespec a, struct timespec b) {
	long ns = a.tv_nsec - b.tv_nsec;
	time_t s = a.tv_sec - b.tv_sec - (ns / 1000000000);
	ns = ns % 1000000000;
	struct timespec sp;
	sp.tv_sec = s;
	sp.tv_nsec = ns;
	return sp;
}

#define LESS_THAN -1
#define EQUAL 0
#define MORE_THAN 1

int compare_timespec(struct timespec a, struct timespec b) {
	if(a.tv_sec < b.tv_sec) {
		return LESS_THAN;
	} else if(a.tv_sec > b.tv_sec) {
		return MORE_THAN;
	} else if(a.tv_nsec < b.tv_nsec) {
		return LESS_THAN;
	} else if(a.tv_nsec > b.tv_nsec) {
		return MORE_THAN;
	} else {
		return EQUAL;
	}
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

void hexdump(void* ptr, int len) {
	fhexdump(stdout, ptr, len);
}

void fhexdump(FILE* f, void* ptr, int len) {
	unsigned char* addr = (unsigned char*) ptr;
	int i;
	for(i=0; i<len; i++) {
		fprintf(f, "%x%c", addr[i], (i%16 || i == 0) ? ' '  : '\n');
	}
}