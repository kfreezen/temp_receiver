#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <signal.h>

#include "xbee.h"
#include "XBeeCommunicator.h"
#include "util.h"
#include "watchdog.h"

using std::deque;

#define COMM_QUIT 255

const int XBeeCommunicator::MAX_CONCURRENT_COMMS = 255;
XBeeCommunicator* XBeeCommunicator::defaultComm = NULL;

void XBeeCommunicator::initDefault(SerialPort* port) {
	XBeeCommunicator::defaultComm = new XBeeCommunicator(port);
}

void XBeeCommunicator::cleanupDefault() {
	delete XBeeCommunicator::defaultComm;
}

XBeeCommunicator::XBeeCommunicator(SerialPort* port) {
	this->dispatchThread = new pthread_t;
	this->handlerThread = new pthread_t;

	this->serialPort = port;
	this->xbeeCommBits = new vector<bool>(XBeeCommunicator::MAX_CONCURRENT_COMMS);
	this->xbeeComms = new XBeeCommStruct [MAX_CONCURRENT_COMMS + 1];
	memset(this->xbeeComms, 0, sizeof(XBeeCommStruct) * (MAX_CONCURRENT_COMMS+1));
}

XBeeCommunicator::~XBeeCommunicator() {
	// Figure out how to get that one thread to exit.

	// These is bad way to do thing, but I'm too lazy to do other way until
	// run into problem.
	//pthread_kill(*this->dispatchThread, SIGTERM);
	//pthread_kill(*this->handlerThread, SIGTERM);

	delete this->dispatchThread;
	delete this->handlerThread;

	delete this->xbeeCommBits;
	delete[] this->xbeeComms;
}

void XBeeCommunicator::startHandler() {
	pthread_mutex_init(&this->handlerThreadMutex, NULL);
	pthread_cond_init(&this->handlerThreadCondition, NULL);

	int err = pthread_create(this->handlerThread, NULL, (void*(*)(void*))XBeeCommunicator::handler, (void*) this);
	
	if(err) {
		string errReason;
		switch(err) {
			case EAGAIN:
				errReason = "EAGAIN:  Insufficient resources.";
				break;
			case EINVAL:
				errReason = "EINVAL:  Invalid settings in attr.";
				break;
			case EPERM:
				errReason = "EPERM:  No permision to allow settings specified in attr.";
				break;
		}

		printf("ERROR:  Failed to start XBee dispatcher thread.  reason: %s\n", errReason.c_str());
	}
}

void XBeeCommunicator::startDispatch() {
	pthread_mutex_init(&this->dispatchThreadMutex, NULL);
	pthread_cond_init(&this->dispatchThreadCondition, NULL);

	int err = pthread_create(this->dispatchThread, NULL, (void*(*)(void*))XBeeCommunicator::dispatcher, (void*) this);

	if(err) {
		string errReason;
		switch(err) {
			case EAGAIN:
				errReason = "EAGAIN:  Insufficient resources.";
				break;
			case EINVAL:
				errReason = "EINVAL:  Invalid settings in attr.";
				break;
			case EPERM:
				errReason = "EPERM:  No permision to allow settings specified in attr.";
				break;
		}

		printf("ERROR:  Failed to start XBee dispatcher thread.  reason: %s\n", errReason.c_str());
	}

	// Now created, or should be anyway.
}

void XBeeCommunicator::waitCommStruct(int id) {
	int commIdInt = id - 1;
	
	this->xbeeComms[commIdInt].waiting++;

	while(1) {
		if(this->xbeeComms[commIdInt].finished == 1) {
			this->xbeeComms[commIdInt].waiting--;
			return;
		} else {
			struct timespec s;
			s.tv_sec = 0;
			s.tv_nsec = 1000000;
			nanosleep(&s, NULL);
		}
	}
}

XBeeCommStruct* XBeeCommunicator::getCommStruct(int id) {
	int commIdInt = id - 1;
	if((*this->xbeeCommBits)[commIdInt] == true) {
		return &this->xbeeComms[commIdInt];
	} else {
		return NULL;
	}
}

void XBeeCommunicator::freeCommStruct(int id) {
	XBeeCommStruct* commStruct = this->getCommStruct(id);
	if(commStruct->replyData != NULL) {
		delete (uint32*) commStruct->replyData;
		commStruct->replyData = NULL;
	}

	if(commStruct->origFrame != NULL) {
		delete commStruct->origFrame;
		commStruct->origFrame = NULL;
	}

	if(commStruct->replyFrame != NULL) {
		delete commStruct->replyFrame;
		commStruct->replyFrame = NULL;
	}
	(*this->xbeeCommBits)[id - 1] = false;
}

int XBeeCommunicator::registerRequest(XBeeCommRequest request) {
	// If request.id is between 1 and 8 including, we need to find one that's functionally identical.
	// (new request.id - 1) % 8 == (old request.id - 1) is the requirement.
	
	int i_init = 0, i_inc = 1;
	if(request.id && request.id < 9) {
		i_init = request.id - 1;
		i_inc = 8;
	}

	int i;
	for(i = i_init; i < this->xbeeCommBits->size(); i += i_inc) {
		if((*this->xbeeCommBits)[i] == false) {
			(*this->xbeeCommBits)[i] = true;
			break;
		}
	}

	if(i==this->xbeeCommBits->size()) {
		i = -1;
	}
	
	this->xbeeComms[i].waiting = 0;

	request.id = i + 1;

	pthread_mutex_lock(&this->dispatchThreadMutex);
	this->dispatchQueue.push_front(request);
	
#ifdef XBEE_COMM_WORKING_TEST
	printf("Registering a request. %p\n", request.callback);
#endif

	pthread_cond_signal(&this->dispatchThreadCondition);
	pthread_mutex_unlock(&this->dispatchThreadMutex);

	return request.id;
}

void* XBeeCommunicator::handler(XBeeCommunicator* comm) {
	bool quit = false;

	while(!quit) {

		bool commStructAllocated = false;

		// Read frame in.
		Frame* frame = new Frame;
		memset(frame, 0, sizeof(Frame));

		XBAPI_ReadFrame(comm->serialPort, frame);

		// Now, we need to determine which commStruct
		// that this one replied to.
		int commId = frame->txStatus.frame_id;
		
		XBeeCommStruct* commStruct;

		if(commId > 0) {
			commStructAllocated = false;
			commStruct = &comm->xbeeComms[commId-1];
		} else {
			commStructAllocated = true;

			// There are no valid commStructs for this, we should just create
			// a stub one.
			commStruct = new XBeeCommStruct;
			memset(commStruct, 0, sizeof(XBeeCommStruct));
			
			commStruct->callback = NULL;
			commStruct->id = 0;
		}
		
		// Give it its reply frame.
		commStruct->replyFrame = frame;
		
		// Figure out which handler to call.
		/* Do it in this fashion:
			First call the user specified handler, if there is any.
			If there is none, or the handler does not handle it,
			handle it with the default handler.
		*/
		int cbRet = 0;
		if(commStruct->callback) {
			printf("callback=%p\n", commStruct->callback);
			cbRet = commStruct->callback(comm, commStruct);
		}

		if(cbRet == 0) {
			cbRet = XBAPI_HandleFrameCallback(comm, commStruct);
		}

		if(cbRet == 1) {
			commStruct->finished = 1;
		}

		// If there is no one waiting, then free the structure
		if(cbRet == 1 && commStruct->waiting == 0 && commStruct->id != 0) {
			// Before we free, we need to check
			// to see if the argument is valid.
			if(commStruct->origFrame) {
				comm->freeCommStruct(commStruct->origFrame->txStatus.frame_id);
			} else {
				printf("commStruct->origFrame = %p\n", commStruct->origFrame);
			}
		}

		if(commStructAllocated) {
			// Go through the commStruct first and delete everything that needs deleting.
			if(commStruct->replyFrame != NULL) {
				delete commStruct->replyFrame;
			}

			delete commStruct;
		}
	}
}

const struct timespec MAX_DISPATCH_WAIT = {0, MAX_DISPATCH_WAIT_NS};

void restartDispatchThread(Watchdog* watchdog) {
	pthread_kill(*watchdog->getThread(), SIGTERM);
	pthread_create(watchdog->getThread(), NULL, (void*(*)(void*)) XBeeCommunicator::dispatcher, watchdog->getUserData());
}

void* XBeeCommunicator::dispatcher(XBeeCommunicator* comm) {
	Watchdog* watchdog = NULL;
	struct timespec wdtTmo;
	wdtTmo.tv_sec = 1;
	wdtTmo.tv_nsec = 0;

	watchdog = new Watchdog("dispatcher", comm->getDispatchThread(), wdtTmo, restartDispatchThread);
	watchdog->setUserData((void*) comm);

	registerWatchdog(watchdog);

	// We basically need an infinite loop that dispatches all requests.

	// We want to send a command every 10 seconds to keep the connection alive.
	time_t lastHeartbeatSent = 0;

	while(1) {
		if(time(NULL) <= lastHeartbeatSent + XBEE_HEARTBEAT_TIMEOUT) {
			XBAPI_Command(comm, API_CMD_ATHV, NULL, FALSE);
			lastHeartbeatSent = time(NULL);
		
			// Let's check the XBee heartbeat status and see if we need to reinitialize the port.
			// Limited to every XBEE_HEARTBEAT_TIMEOUT seconds so that the program doesn't go crazy
			// when things do fail.
			if(comm->heartbeatExpired()) {
				comm->serialPort->reinit();
				Logger_Print(INFO, time(NULL), "Serial port being reinitialized due to a heartbeat failure.\n");
			}

		}

		while(!comm->dispatchQueue.empty()) {
			printf("comm->dispatchQueue.size() = %d\n", comm->dispatchQueue.size());
			pthread_mutex_lock(&comm->dispatchThreadMutex);
			XBeeCommRequest request = comm->dispatchQueue.back();
			comm->dispatchQueue.pop_back();
			pthread_mutex_unlock(&comm->dispatchThreadMutex);

			// Here we need to find a free comm slot.
			int commId;

			commId = request.id;
			if(commId == 0) {
				// Not a good solution.
				continue;
			}

			printf("request.callback=%p\n", request.callback);

			comm->xbeeComms[commId-1].callback = request.callback;
			comm->xbeeComms[commId-1].id = commId;

			switch(request.commType) {
				case COMM_COMMAND: {
					unsigned short dest = *((unsigned short*)(&request.destination));

					XBAPI_CommandInternal(comm->serialPort, dest, (unsigned*) request.data, request.dataLength, commId, &comm->xbeeComms[commId-1]);
				} break;

				case COMM_TRANSMIT:
					XBAPI_TransmitInternal(comm->serialPort, (XBeeAddress*) request.destination, request.data, commId, request.dataLength, &comm->xbeeComms[commId-1]);

					// Done with request, so we should probably delete request.data.
					delete[] request.data;
					
					break;
			}
		}

		watchdog->reset();

		pthread_mutex_lock(&comm->dispatchThreadMutex);

		struct timespec absTime = add_timespec(now(), MAX_DISPATCH_WAIT);
		pthread_cond_timedwait(&comm->dispatchThreadCondition, &comm->dispatchThreadMutex, &absTime);
		pthread_mutex_unlock(&comm->dispatchThreadMutex);
	}
}

void XBeeCommunicator::retry(XBeeCommStruct* commStruct) {
	if(commStruct->replyFrame != NULL) {
		delete commStruct->replyFrame;
		commStruct->replyFrame = NULL;
	}

	int length = 0;
	length |= (commStruct->origFrame->tx.rev0.length[0] << 8);
	length |= (commStruct->origFrame->tx.rev0.length[1]);
	commStruct->retries ++;
	this->serialPort->write(commStruct->origFrame, length + 4);
}

