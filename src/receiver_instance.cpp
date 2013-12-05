#include "receiver_instance.h"

#include <cstdio>

extern FILE* __stdout_log;

ReceiverInstance::ReceiverInstance(SerialPort* port) {
	ReceiverInstance::port = port;
	ReceiverInstance::thread = NULL;
}

void ReceiverInstance::run() {
	if(thread) {
		return;
	}

	pthread_t* thread = new pthread_t;
	int pthreadErr = pthread_create(thread, NULL, (void* (*)(void*))ReceiverInstance::instance, this);
	if(pthreadErr == EAGAIN) {
		fprintf(__stdout_log, "No resources for ReceiverInstance thread.\n");
	} else if(pthreadErr == 0) {
		ReceiverInstance::thread = thread;
		ReceiverInstance::instanceId = ReceiverInstance::nextInstanceId++;
	} else if(pthreadErr == EINVAL) {
		fprintf(__stdout_log, "A value specified by attr is invalid (ReceiverInstance)\n");
	} else {
		fprintf(__stdout_log, "Something went wrong in ReceiverInstance.\n");
	}
}

void* ReceiverInstance::instance(ReceiverInstance* _this) {
	ReceiverInstance* receiverInstance = _this;

}