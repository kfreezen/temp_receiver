#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <pthread.h>
#include <cstdio>

class Watchdog;

typedef void (*RestartCallback)(Watchdog* watchdog);

class Watchdog {
public:
	Watchdog(const char* id, pthread_t* thread, struct timespec timeout, RestartCallback callback);
	~Watchdog() {
		delete[] id;
	}

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

	void setUserData(void* p) {
		this->userData = p;
	}

	void* getUserData() {
		return this->userData;
	}

	// Reset WDT for this thread.
	void reset();

	// This tells us whether the WDT for this thread has expired. 
	bool expired();

	// This is to reduce number of add_timespec calls by watchdog thread.
	struct timespec getCachedExpireTime();
	struct timespec getCachedCurTime() {
		return this->cachedCurTime;
	}

	void restartThread();
private:
	char* id;

	struct timespec timeout;

	pthread_t* thread;
	RestartCallback callback;

	struct timespec lastReset;
	void* userData;

	struct timespec cachedExpireTime;
	struct timespec cachedCurTime;

};

void startWatchdogThread();
void registerWatchdog(Watchdog* watchdog);
void watchdogQuit();
#endif