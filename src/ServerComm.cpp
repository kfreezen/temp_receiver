#include "ServerComm.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <pthread.h> 

#include <cstdio>
#include <cstring>

#include <string>
#include <sstream>

#include "logger.h"
#include "settings.h"

#include "curl.h"
#include "util.h"

#define SERVERCOMM_DEBUG 0

pthread_mutex_t* logMutex = NULL;

ServerComm* ServerComm::comm = NULL;

ServerComm::ServerComm() {
	this->m_isUsable = false;

	// Make sure logMutex is not NULL.
	if(logMutex == NULL) {
		logMutex = new pthread_mutex_t;
		pthread_mutex_init(logMutex, NULL);
	}

	pthread_mutex_init(&this->socketMutex, NULL);
	this->heartbeatThread = NULL;

	if(ServerComm::comm == NULL) {
		ServerComm::comm = this;
	}

	// Initialize our condition variable for sends.
	pthread_cond_init(&this->sendCond, NULL);
	// And its mutex.
	pthread_mutex_init(&this->sendCondMutex, NULL);

}

void ServerComm::start() {
	this->commThread = new pthread_t;
	pthread_create(this->commThread, NULL, ServerComm::CommThread, (void*) this);
}

RSCPContent::RSCPContent(RSCPContent* content) {
	if(content->assembledContent != NULL) {
		this->assembledContent = new char[strlen(content->assembledContent) + 1];
		strcpy(this->assembledContent, content->assembledContent);
	} else {
		this->assembledContent = NULL;
	}

	if(content->request != NULL) {
		this->request = new char[strlen(content->request) + 1];
		strcpy(this->request, content->request);
	} else {
		this->request = NULL;
	}

	map<string, char*>::iterator itr;
	for(itr = content->contentLines.begin(); itr != this->contentLines.end(); itr++) {
		char* data = new char[strlen(itr->second) + 1];
		this->contentLines[itr->first] = data;
	}
}

#define INITIAL_BYTES 256

void RSCPContent::loadFromString(const char* protected_str) {
	this->clear();

	char* str = new char[strlen(protected_str) + 1];
	strcpy(str, protected_str);

	char* savedptr = NULL;
	char* requestLine = strtok_r(str, "\n", &savedptr);
	this->setRequest(requestLine);

	while(1) {
		char* line = strtok_r(NULL, "\n", &savedptr);
		if(line == NULL) {
			break;
		}

		if(strlen(line) > 0) {
			this->addLineString(line);
		}
	}

	delete[] str;
}

int RSCPContent::loadFromComm(ServerComm* comm) {
	int err = this->loadFromSocket(comm->getSocketFd());
	if(err == UNHANDLED_RECV_ERROR) {
		comm->isDisconnected = true;
		comm->disconnectCause = "unknown";
	} else if(err == SOFTWARE_DISCONNECT) {
		comm->isDisconnected = true;
		comm->disconnectCause = "software";
	}
}

int RSCPContent::loadFromSocket(int socketFd) {
	printf("loadFromSocket\n");

	// Why am I using a CURLBuffer?  Because it has properties I want in this function.
	CURLBuffer* buffer = new CURLBuffer;
	buffer->init(INITIAL_BYTES + 1);

	while(1) {
		if(buffer->capacity - buffer->length < INITIAL_BYTES / 8) {
			buffer->extend(INITIAL_BYTES);
		}

		int receivedBytes = recv(socketFd, buffer->buffer+buffer->length, buffer->capacity - buffer->length, 0);
		printf("receivedBytescheck = %d\n", receivedBytes);

		if(receivedBytes < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				// No data in the buffer, wait about 100 ms.
				nsleep(100 * 1000 * 1000);
				continue;
			} else {
				Logger_Print(ERROR, time(NULL), "recv() error in getPacket() %s\n", strerror(errno));
				return UNHANDLED_RECV_ERROR;
			}
		} else if(receivedBytes == 0) {
			// This indicates that the socket was disconnected, most likely by software.
			return SOFTWARE_DISCONNECT;
		} else {
			buffer->length += receivedBytes;
			
			// We need to see if we have received end-of-content, which is two newlines in a row.
			if(buffer->length >= 2 && buffer->buffer[buffer->length-2] == '\n' && buffer->buffer[buffer->length-1] == '\n') {
				// Finished with buffer.
				break;
			}
		}

	}

	#ifdef SERVERCOMM_DEBUG
	printf("received-content=%s\n", buffer->buffer);
	#endif

	this->loadFromString(buffer->buffer);

	delete buffer;

	return SUCCESS;
}

void ServerComm::cleanup() {
	this->m_isUsable = false;

	this->clearErrors();
	if(this->heartbeatThread != NULL) {
		this->heartbeatThreadComm = ServerComm::QUIT;
		while(this->heartbeatThreadComm != ServerComm::DONE) {
		}

		delete this->heartbeatThread;
		this->heartbeatThread = NULL;
	}

	this->heartbeatThreadComm = 0;
}

#define ERRSTR_SIZE 256
void* ServerComm::CommThread(void* arg) {
	ServerComm* comm = (ServerComm*) arg;

doRetry:
	struct addrinfo* result = NULL;
	struct addrinfo hint;

	char* portStr = NULL;

	while(1) {
		if(result != NULL) {
			freeaddrinfo(result);
			result = NULL;
		}

		if(portStr != NULL) {
			delete[] portStr;
			portStr = NULL;
		}

		memset(&hint, 0, sizeof(hint));

		comm->cleanup();

		comm->socketFd = socket(AF_INET, SOCK_STREAM, 0);

		if(comm->socketFd < 0) {
			fprintf(stderr, "socket error %s\n", strerror(errno));
			Logger_Print(ERROR, time(NULL), "socket error %s\n", strerror(errno));
			return NULL;
		}

		// Fill out hints.
		hint.ai_family = AF_INET;
		hint.ai_socktype = SOCK_STREAM;

		portStr = new char[32];
		snprintf(portStr, 32, "%d", ServerComm::SERVER_PORT);

		int gaiError = getaddrinfo(Settings::get("server").c_str(), portStr, &hint, &result);
		if(gaiError != 0) {
			Logger_Print(ERROR, time(NULL), "getaddrinfo error %s\n", gai_strerror(gaiError));
			
			sleep(RETRY_WAIT);
			continue;
		}

		if(connect(comm->socketFd, result->ai_addr, result->ai_addrlen) < 0) {
			char* errstr = getErrorString();
			Logger_Print(ERROR, time(NULL), "Connection failed.  error %s.  %s, %s\n", errstr, Settings::get("server").c_str(), portStr);
			delete[] errstr;

			sleep(RETRY_WAIT);
			continue;
		} else {
			
			// Now that the connect has been completed, we place
			// the socket in nonblocking mode.
			int opts = fcntl(comm->socketFd, F_GETFL);
			if(opts < 0) {
				char* errstr = getErrorString();
				Logger_Print(ERROR, time(NULL), "fcntl(F_GETFL) error %s\n", errstr);
				delete[] errstr;

				return NULL;
			}

			opts |= O_NONBLOCK;
			if(fcntl(comm->socketFd, F_SETFL, opts) < 0) {
				Logger_Print(ERROR, time(NULL), "fcntl(F_GETFL) error %s\n", strerror(errno));
				return NULL;
			}

			// Now we need to send our connection packet.

			// Set up the connection packet.
			RSCPContent content;
			content.setRequest(CONNECT);

			extern string receiverId;
			content.addLine("id", receiverId.c_str());

			content.sendToSocket(comm->socketFd);

			/*char* contentStr = content.getContentAsString();

			// Send the connection content.
			send(comm->socketFd, contentStr, strlen(contentStr), 0);
			delete[] contentStr;*/

			content.loadFromComm(comm);
			if(comm->isDisconnected == true) {
				// Let's wait thirty seconds and retry it.
				close(comm->socketFd);
				sleep(30);
				continue;
			}

			const char* ack = content.getLineData("ack");
			if(ack == NULL || strcmp(ack, "success")) {
				// Something's gone wrong.
				char* errorContent = content.getContentAsString();
				Logger_Print(ERROR, time(NULL), "ack was not successful.\n%s\n", errorContent);
				delete[] errorContent;
			}

			content.clear();

			// Here we are, our connection handshake is done.
			comm->lastHeartbeat = time(NULL);

			break;
		}
	}
	
	// Clean up addrinfo 
	freeaddrinfo(result);
	// We don't need portStr anymore
	delete[] portStr;

	if(comm->heartbeatThread != NULL) {
		// close the heartbeat thread.
		comm->heartbeatThreadComm = ServerComm::QUIT;
		while(comm->heartbeatThreadComm != ServerComm::DONE) {

		}

		delete comm->heartbeatThread;
		comm->heartbeatThread = NULL;
	}

	comm->heartbeatThread = new pthread_t;

	// Spawn our heartbeat thread.
	pthread_create(comm->heartbeatThread, NULL, ServerComm::HeartbeatThread, comm);

	int num_einvals = 0;

	// The fact that we got here means that it is usable.
	while(1) {
		// First we check to see if the socket has been disconnected.
		if(comm->isDisconnected == true) {
			goto doRetry;
		} else {
			comm->m_isUsable = true;
		}

		// Go through our deque of contents to send.
		while(!comm->toSend.empty()) {
			pthread_mutex_lock(&comm->sendLock);
			RSCPContent* content = comm->toSend.front();
			comm->toSend.pop_front();
			pthread_mutex_unlock(&comm->sendLock);

			content->sendToSocket(comm->socketFd);

			delete content;
		}

		struct timespec abstime;
		struct timespec timeToWait;
		timeToWait.tv_sec = 1;
		timeToWait.tv_nsec = 0;

		clock_gettime(CLOCK_REALTIME, &abstime);
	
		pthread_mutex_lock(&comm->sendCondMutex);

		abstime = add_timespec(abstime, timeToWait);

		int timedwait_error = pthread_cond_timedwait(&comm->sendCond, &comm->sendCondMutex, &abstime);
		pthread_mutex_unlock(&comm->sendCondMutex);
		
		if(timedwait_error != 0) {
			// Make sure the error isn't timed out, because timedout is innocuous
			// in this usage.
			if(timedwait_error != ETIMEDOUT) {
				if(timedwait_error == EINVAL) {
					printf("%d einvals have now occurred.  Attempting fix\n", ++num_einvals);
					// "EINVAL The value specified by cond, mutex, or abstime is invalid" (from man page),
					// so we should try reinitializing the condition and the mutex variables.
					pthread_mutex_lock(&comm->sendCondMutex);
					int destroy_err = pthread_mutex_destroy(&comm->sendCondMutex);
					if(destroy_err != 0) {
						Logger_Print(ERROR, time(NULL), "Error destroying mutex.  %d\n", destroy_err);
					}

					destroy_err = pthread_cond_destroy(&comm->sendCond);
					if(destroy_err != 0) {
						Logger_Print(ERROR, time(NULL), "Error destroying cond variable.  %d\n", destroy_err);
					}

					pthread_mutex_init(&comm->sendCondMutex, NULL);
					pthread_cond_init(&comm->sendCond, NULL);
				}
			}
		}
	}

}

void* ServerComm::HeartbeatThread(void* arg) {
	ServerComm* comm = (ServerComm*) arg;

	RSCPContent heartbeat;
	heartbeat.setRequest(HEARTBEAT);

	while(comm->heartbeatThreadComm != QUIT) {
		int secondsToSleep = comm->lastHeartbeat + ServerComm::HEARTBEAT_SECONDS - time(NULL);
		sleep(secondsToSleep);

		pthread_mutex_lock(&comm->socketMutex);
		if(comm->isDisconnected == true) {
			// wait a few seconds for the main loop to attempt to reestablish the socket.
			sleep(10);
			continue;
		} else {
			printf("doing send\n");
		}

		if(heartbeat.sendToSocket(comm->socketFd) == -1) {
			if(errno == EPIPE) {
				comm->isDisconnected = true;
				comm->disconnectCause = "unknown";
			}
		} else {
			comm->lastHeartbeat = time(NULL);
		}
		pthread_mutex_unlock(&comm->socketMutex);
		
		comm->lastHeartbeat = time(NULL);
	}

	comm->heartbeatThreadComm = ServerComm::DONE;
}

void RSCPContent::sendTo(ServerComm* comm) {
	RSCPContent* copied = new RSCPContent(this);
	comm->sendContent(copied);
}

int RSCPContent::sendToSocket(int socketFd) {
	char* contentStr = this->getContentAsString();

	// TODO:  Do some error handling.
	if(send(socketFd, contentStr, strlen(contentStr), MSG_NOSIGNAL) == -1) {
		printf("send to socket failed.  %d\n", errno);
		
		delete[] contentStr;
		return -1;
	} else {
		delete[] contentStr;
		return 0;
	}
}

const char* RSCPContent::getRequest() {
	return this->request;
}

void RSCPContent::setRequest(const char* request) {
	if(request == NULL) {
		return;
	}

	if(this->request != NULL) {
		delete[] this->request;
	}

	this->request = new char[strlen(request) + 1];
	strcpy(this->request, request);

	this->assembledContentOutOfDate = true;
}

void RSCPContent::addLine(const char* lineId, const char* lineData) {
	char* _lineData = new char[strlen(lineData) + 1];
	strcpy(_lineData, lineData);

	this->contentLines[string(lineId)] = _lineData;
	this->assembledContentOutOfDate = true;
}

void RSCPContent::addLineString(const char* lineString) {
	char* str = new char[strlen(lineString) + 1];
	strcpy(str, lineString);

	// First we fetch the ID.
	int i;
	int strlength = strlen(str);
	for(i = 0; i < strlength; i++) {
		if(str[i] == ':') {
			str[i] = '\0';
			break;
		}
	}

	i++;
	char* idStart = &str[0];

	// Zoom through any whitespace
	for(; i < strlength; i++) {
		if(!isspace(str[i])) {
			break;
		}
	}

	// Everything up to the '\n' or '\0' is data.
	char* dataStart = &str[i];
	for(; i < strlength; i++) {
		if(str[i] == '\n' || str[i] == '\0') {
			str[i] = 0;
			break;
		}
	}

	char* data = new char[strlen(dataStart) + 1];
	strcpy(data, dataStart);

	this->contentLines[string(idStart)] = data;
	this->assembledContentOutOfDate = true;

	delete[] str;
}

char* RSCPContent::getContentAsString() {
	bool reassemble = false;
	if(this->assembledContentOutOfDate || this->assembledContent == NULL) {
		reassemble = true;
	}

	if(!reassemble) {
		char* ret = new char[strlen(this->assembledContent)+1];
		strcpy(ret, this->assembledContent);
		return ret;
	} else {
		// We have to reassemble.
		// Delete the old assembledContent if it needs deleting.
		if(this->assembledContent != NULL) {
			delete[] this->assembledContent;
		}

		// Ok, we want to get all the line strings now.
		int strarrayLength = this->contentLines.size();
		int estimatedContentLength = 32; // This 32 will give us some extra padding.
		char** strarray = new char* [strarrayLength];

		int i;
		map<string, char*>::iterator lineItr;
		for(i = 0, lineItr = this->contentLines.begin(); lineItr != this->contentLines.end(); i++, lineItr++) {
			char* line = new char[lineItr->first.length() + 2 + strlen(lineItr->second) + 2];
			strcpy(line, lineItr->first.c_str());
			strcat(line, ": ");
			strcat(line, lineItr->second);

			strarray[i] = line;

			estimatedContentLength += strlen(strarray[i]) + 2;
		}

		// Start assembling the content.
		char* assembledContent = new char[estimatedContentLength];
		int assembledContentLength = 0;
		// Add request line.
		strcpy(&assembledContent[assembledContentLength], this->request);
		assembledContentLength += strlen(this->request);
		strcpy(&assembledContent[assembledContentLength], "\n");
		assembledContentLength ++;

		// Add lines.
		for(i = 0; i < strarrayLength; i++) {
			strcpy(&assembledContent[assembledContentLength], strarray[i]);
			assembledContentLength += strlen(strarray[i]);
			strcpy(&assembledContent[assembledContentLength], "\n");
			assembledContentLength ++;
		}

		// Add the last LF.
		strcpy(&assembledContent[assembledContentLength], "\n");
		assembledContentLength ++;

		this->assembledContent = assembledContent;
		this->assembledContentOutOfDate = false;

		for(i=0; i < strarrayLength; i++) {
			delete[] strarray[i];
		}

		delete[] strarray;

		char* ret = new char[assembledContentLength + 1];
		strcpy(ret, this->assembledContent);

		return ret;
	}
}

void ServerCommTests() {
	// Write tests!!!
	// RSCPContent test first.
	RSCPContent content;
	content.setRequest("TEST");
	content.addLine("test0", "test");

	char* str = content.getContentAsString();
	printf("1 content string %p\n%s\n", str, str);
	delete[] str;

	// Testing our time saving mechanism.
	str = content.getContentAsString();
	printf("2 content string %p\n%s\n", str, str);
	delete[] str;

	content.addLine("test1", "testagain");

	str = content.getContentAsString();
	printf("3 content string %p\n%s\n", str, str);
	delete[] str;

	content.addLineString("test2: linestringtest");

	str = content.getContentAsString();
	printf("4 content string %p\n%s\n", str, str);
	delete[] str;

	content.loadFromString("TEST\ntest3: loadfromstringtest\ntest4:   test4\ntest5:\ttest5\n\n");

	str = content.getContentAsString();
	printf("5 content string (loaded) %p\n%s\n", str, str);
	delete[] str;

	content.clear();
	content.setRequest("TEST");

	str = content.getContentAsString();
	printf("6 content string (requestonly) %p\n%s\n", str, str);
	delete[] str;

	// Now we need to set up a server comm test.
	string oldServer = Settings::get("server");

	Settings::set("server", "localhost");
	ServerComm* serverComm = new ServerComm;

	serverComm->start();
	printf("started serverComm\n");

	RSCPContent* logContent = new RSCPContent;
	logContent->setRequest("LOG");
	logContent->addLine("message", "testing LOG request.");
	logContent->addLine("level", "INFO");

	char* tstamp = new char[32];
	snprintf(tstamp, 32, "%ld", time(NULL));
	logContent->addLine("timestamp", tstamp);
	delete[] tstamp;

	serverComm->sendContent(logContent);

	while(1) {
		sleep(1);
	}
}