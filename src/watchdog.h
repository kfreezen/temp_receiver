#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <pthread.h>
#include <cstdio>

class Watchdog;

typedef void (*RestartCallback)(Watchdog* watchdog);

class Watchdog {
public:
	Watchdog(const char* id, pthread_t* thread, struct timespec timeout, RestartCallback callback);

	const char* getId() {
		return this->id;
	}

	struct timespec getTimeout() {
		return this->timeout;
	}

	struct timespec getLastReset() {
		return this->lastReset;
	}

	pthread_t* getThread() {
		return this->thread;
	}

	RestartCallback getCallback() {
		return this->callback;
	}

	// Reset WDT for this thread.
	void reset();

	// This tells us whether the WDT for this thread has expired. 
	bool expired();

	void restartThread();
private:
	char* id;

	struct timespec timeout;

	pthread_t* thread;
	RestartCallback callback;

	struct timespec lastReset;
};

void startWatchdogThread();
void registerWatchdog(Watchdog* watchdog);

#endif