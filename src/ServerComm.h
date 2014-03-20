#ifndef SERVERCOMM_H
#define SERVERCOMM_H

#include <pthread.h>

// I thinks the best way to implement this is for it to have its own thread.

class ServerComm {
public:
	ServerComm();
	~ServerComm();

	void start();

	static void* CommThread(void* arg);
private:
	const static int SERVER_PORT = 14440;

	int socketFd;

	pthread_t* commThread;
};

#endif