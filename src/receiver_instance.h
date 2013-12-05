#ifndef RECEIVER_INSTANCE_H
#define RECEIVER_INSTANCE_H

#include "serial.h"
#include <pthread.h>

// Just a note so you don't try to decipher this wrongly.
// This is not currently used.  If you come across references in
// the code besides this header and its accompanying C++ file,
// then these comments are out of date and should be removed.

class ReceiverInstance {
public:
	ReceiverInstance(SerialPort* port);

	void run();

private:
	static int nextInstanceId;

	int instanceId;
	SerialPort* port;
	pthread_t* thread;

	pthread_t* getThread() {
		return thread;
	}

	static void* instance(ReceiverInstance* _this);
};

#endif