#ifndef SERVERCOMM_H
#define SERVERCOMM_H

#include <pthread.h>
#include <vector>
#include <map>
#include <string>

#include <cstring>
#include <cctype>
#include <ctime>
#include <deque>
#include <iterator>

using std::map;
using std::string;
using std::vector;
using std::deque;

// I thinks the best way to implement this is for it to have its own thread.

class ServerComm;

class RSCPContent {
public:
	RSCPContent() {
		this->assembledContent = NULL;
		this->request = NULL;
		this->assembledContentOutOfDate = true;
	}

	RSCPContent(RSCPContent* content);

	~RSCPContent() {
		this->clear();
	}

	void clear() {
		if(this->assembledContent != NULL) {
			delete[] this->assembledContent;
			this->assembledContent = NULL;
		}

		if(this->request != NULL) {
			delete[] this->request;
			this->request = NULL;
		}

		map<string, char*>::iterator itr;
		for(itr = this->contentLines.begin(); itr != this->contentLines.end(); std::advance(itr, 1)) {
			// Delete our lines one by one. 
			delete[] itr->second;
		}

		this->contentLines.clear();
	}

	const char* getRequest();
	void setRequest(const char* request);

	void addLine(const char* lineId, const char* lineData);
	void addLineString(const char* lineStr);

	const char* getLineData(string key) {
		return this->contentLines[key];
	}

	char* getContentAsString();

	void loadFromString(const char* str);

	int loadFromComm(ServerComm* comm);
	int loadFromSocket(int socketFd);

	void sendTo(ServerComm* comm);

	int sendToSocket(int socketFd);

private:
	bool assembledContentOutOfDate;
	char* assembledContent;

	char* request;

	map<string, char*> contentLines;
};

#define RETRY_WAIT 30

class ServerComm {
public:
	ServerComm();
	~ServerComm();

	void start();

	static void* CommThread(void* arg);
	static void* HeartbeatThread(void* arg);

	bool isDisconnected;
	const char* disconnectCause;

	void clearErrors() {
		this->isDisconnected = false;
		this->disconnectCause = "";
	}

	int getSocketFd() {
		return this->socketFd;
	}

	void sendContent(RSCPContent* content) {
		pthread_mutex_lock(&this->sendLock);
		this->toSend.push_back(content);
		pthread_mutex_unlock(&this->sendLock);

		pthread_mutex_lock(&this->sendCondMutex);
		pthread_cond_signal(&this->sendCond);
		pthread_mutex_unlock(&this->sendCondMutex);
	}

	bool isUsable() {
		return this->m_isUsable;
	}

	static ServerComm* comm;

private:
	// This basically tells the program whether send-request will get sent.
	bool m_isUsable;

	void cleanup();

	const static int SERVER_PORT = 14440;
	const static int HEARTBEAT_SECONDS = 10;

	time_t lastHeartbeat;
	int socketFd;

	pthread_t* commThread;

	const static int QUIT = 255;
	const static int DONE = 256;

	int heartbeatThreadComm;
	pthread_t* heartbeatThread;

	pthread_mutex_t socketMutex;

	deque<RSCPContent*> toSend;
	pthread_mutex_t sendLock;

	pthread_cond_t sendCond;
	pthread_mutex_t sendCondMutex;

	int version;
};

typedef struct {
	char** strings;
	int num;
} StringArray;

// Requests
#define CONNECT "CONNECT"
#define HEARTBEAT "HEARTBEAT"
#define LOG "LOG"

#define UNHANDLED_RECV_ERROR -2
#define SOFTWARE_DISCONNECT -1
#define SUCCESS 0



void ServerCommTests();

#endif