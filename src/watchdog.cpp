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

	struct timespec timeoutTime = add_timespec(this->lastReset, this->timeout);

	if(curTime.tv_sec >= timeoutTime.tv_sec && curTime.tv_nsec > timeoutTime.tv_nsec) {
		return true;
	} else {
		return false;
	}
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

void* WatchdogThread(void* arg);

void startWatchdogThread() {
	if(watchdogEnabled) {
		pthread_create(&watchdogThread, NULL, WatchdogThread, NULL);
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
	while(watchdogComm != WATCHDOG_QUIT) {
		vector<Watchdog*>::iterator itr;

		struct timespec minTime;
		minTime.tv_sec = ~0;
		minTime.tv_nsec = 9999999;
		// OK, we want to figure out the minimum time we need to wait
		for(itr = watchdogs.begin(); itr < watchdogs.end(); itr++) {
			if((*itr)->expired() == true) {
				Logger_Print(ERROR, time(NULL), "WDT calling restartThread on id:%s\n", (*itr)->getId());
				(*itr)->restartThread();

				// The current itr is now pointing to a dead Watchdog, so delete it.
				delete *itr;
				itr = watchdogs.erase(itr);
				continue;
			}

			struct timespec absTimeout = add_timespec((*itr)->getLastReset(), (*itr)->getTimeout());
			struct timespec curTime;

			clock_gettime(CLOCK_REALTIME, &curTime);
			struct timespec timeLeft = sub_timespec(absTimeout, curTime);
			if(compare_timespec(timeLeft, minTime) < 0) {
				minTime = timeLeft;
			}
		}

		pthread_mutex_lock(&watchdogsCondMutex);

		// It doesn't matter if this gets interrupted by something else.
		struct timespec curTime = now();
		minTime = add_timespec(curTime, minTime);
		pthread_cond_timedwait(&watchdogsCond, &watchdogsCondMutex, &minTime);
		
		pthread_mutex_unlock(&watchdogsCondMutex);
	}
}