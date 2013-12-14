#ifndef UTIL_H
#define UTIL_H

#include <ctime>

struct timespec now();
struct timespec add_timespec(struct timespec a, struct timespec b);

#endif
