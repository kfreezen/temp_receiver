#include "util.h"

#include <ctime>

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
