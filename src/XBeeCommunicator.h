#ifndef XBEECOMMUNICATOR_H
#define XBEECOMMUNICATOR_H

#include <deque>
#include <vector>

#include <pthread.h>

#define MAX_DISPATCH_WAIT_NS 250000000
extern const struct timespec MAX_DISPATCH_WAIT;

using namespace std;

typedef struct __XBeeCommStruct XBeeCommStruct;
union __Frame;

class XBeeCommunicator;

typedef int (*XBeeHandleCallback)(XBeeCommunicator* comm, XBeeCommStruct* commStruct);

struct __XBeeCommStruct {
	XBeeHandleCallback callback;
	union __Frame* origFrame;
	union __Frame* replyFrame;
	
	void* replyData;
	int replyDataLength;

	int retries;
	int finished, waiting;
};

typedef struct {
	XBeeHandleCallback callback; // the handler will use this to do the lower-level work of handling, and determining what to do after a failure.

	int commType; // uses the API definitions for frame types.
	
	void* destination; // Meaning varies depending on commType
	void* data; // Is interpreted multiple ways, based on the communication type.  NULL means no data.
	int dataLength; // Length of the data.  0 means no data.

	int id; // XBee frame identification.
} XBeeCommRequest;

class XBeeCommunicator {
public:
	const static int MAX_CONCURRENT_COMMS;

	XBeeCommunicator(SerialPort* port);
	~XBeeCommunicator();

	// Starts dispatcher thread.
	void startDispatch();

	void startHandler();

	int registerRequest(XBeeCommRequest request);

	void retry(XBeeCommStruct* commStruct);
	
	// Use this when waiting on a communication structure to be filled.
	void waitCommStruct(int id);
	
	XBeeCommStruct* getCommStruct(int id);

	// Use this when done with a comm struct.
	void freeCommStruct(int id);
	
	SerialPort* getSerialPort() {
		return this->serialPort;
	}

	static void initDefault(SerialPort* port);
	static XBeeCommunicator* getDefault() {
		return XBeeCommunicator::defaultComm;
	}

	static void cleanupDefault();

private:
	static XBeeCommunicator* defaultComm;

	vector<bool>* xbeeCommBits;
	XBeeCommStruct* xbeeComms;

	deque<XBeeCommRequest> dispatchQueue;
	
	pthread_t dispatchThread;
	pthread_mutex_t dispatchThreadMutex;
	pthread_cond_t dispatchThreadCondition;

	pthread_t handlerThread;
	pthread_mutex_t handlerThreadMutex;
	pthread_cond_t handlerThreadCondition;

	SerialPort* serialPort;

	// These two are called by C functions (pthread), so they need to be static class methods.
	static void* dispatcher(XBeeCommunicator* comm);
	static void* handler(XBeeCommunicator* comm);
};


#endif
