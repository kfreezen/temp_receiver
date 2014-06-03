#include "watchdog.h"
#include "util.h"
#include "logger.h"

#include <cstring>
#include <vector>

using namespace std;

bool watchdogEnabled = true;

Watchdog::Watchdog(const char* id, pthread_t* thread, struct timespec timeout, RestartCallback callback) {
	this->id = NULL;

	if(id == NULL) {
		id = ""; // Reassign it to an empty string.
	}

	this->id = new char[strlen(id) + 1];
	strcpy(this->id, id);
	printf("wid=%x, %s\n", this->id, this->id);

	this->thread = thread;
	this->timeout = timeout;
	this->callback = callback;

	this->reset();
}

void Watchdog::reset() {
	clock_gettime(CLOCK_REALTIME, &this->lastReset);
}

bool Watchdog::expired() {
	struct timespec curTime;
	clock_gettime(CLOCK_REALTIME, &curTime);

	this->cachedCurTime = curTime;

	struct timespec timeoutTime = add_timespec(this->lastReset, this->timeout);
	this->cachedExpireTime = timeoutTime;

	if(curTime.tv_sec >= timeoutTime.tv_sec && curTime.tv_nsec > timeoutTime.tv_nsec) {
		return true;
	} else {
		return false;
	}
}

struct timespec Watchdog::getCachedExpireTime() {
	return this->cachedExpireTime;
}

void Watchdog::restartThread() {
	if(this->callback != NULL) {
		this->callback(this);
	}
}

vector<Watchdog*> watchdogs;
pthread_mutex_t watchdogsCondMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t watchdogsCond = PTHREAD_COND_INITIALIZER;

pthread_t watchdogThread;
bool watchdogThreadRunning = false;

void* WatchdogThread(void* arg);

void startWatchdogThread() {
	if(watchdogEnabled && !watchdogThreadRunning) {
		pthread_create(&watchdogThread, NULL, WatchdogThread, NULL);
		watchdogThreadRunning = false;
	}
}

void registerWatchdog(Watchdog* watchdog) {
	if(watchdogEnabled) {
		watchdogs.push_back(watchdog);
		pthread_cond_signal(&watchdogsCond);
	}
}

#define WATCHDOG_QUIT 255

int watchdogComm = 0;

void watchdogQuit() {
	watchdogComm = WATCHDOG_QUIT;
}

void* WatchdogThread(void* arg) {

	vector<Watchdog*>::iterator itr;
	Watchdog* minWatchdog;

	struct timespec pollTime;

	pollTime.tv_sec = 0;
	pollTime.tv_nsec = 5000000;

	while(watchdogComm != WATCHDOG_QUIT) {
		
		// These should probably be moved outside the while loop.
		struct timespec absTimeout, curTime;

		// OK, we want to figure out the minimum time we need to wait
		for(itr = watchdogs.begin(); itr < watchdogs.end(); itr++) {
			if((*itr)->expired() == true) {
				Logger_Print(ERROR, time(NULL), "WDT calling restartThread on id:%s\n", (*itr)->getId());
				(*itr)->restartThread();

				// The current itr is now pointing to a dead Watchdog, so delete it.
				delete *itr;
				itr = watchdogs.erase(itr);
			}
		}

		nanosleep(&pollTime, NULL);
	}
}