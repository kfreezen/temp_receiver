#include "ServerComm.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <errno.h>
#include <pthread.h> 

#include <cstdio>
#include <cstring>
#include "logger.h"
#include "settings.h"

pthread_mutex_t* logMutex = NULL;

ServerComm::ServerComm() {
	// Make sure logMutex is not NULL.
	if(logMutex == NULL) {
		logMutex = new pthread_mutex_t;
		pthread_mutex_init(logMutex, NULL);
	}
}

void ServerComm::start() {

}

// TODO:  Create a function which takes input from a socket, and gives an array of lines.

#define ERRSTR_SIZE 256
void* ServerComm::CommThread(void* arg) {
	ServerComm* comm = (ServerComm*) arg;

	struct sockaddr_in serv_addr;
	comm->socketFd = socket(AF_INET, SOCK_STREAM, 0);

	if(comm->socketFd < 0) {
		fprintf(stderr, "socket error %s\n", strerror(errno));
		Logger_Print(time(NULL), "socket error %s\n", strerror(errno));
		return NULL;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(ServerComm::SERVER_PORT);

	if(inet_pton(AF_INET, Settings::get("server").c_str(), &serv_addr.sin_addr) <= 0) {
		fprintf(stderr, "inet_pton error %s\n", strerror(errno));
		Logger_Print(time(NULL), "inet_pton error %s\n", strerror(errno));
		return NULL;
	}

	if(connect(comm->socketFd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "Connection failed.  error %s\n", strerror(errno));
		Logger_Print(time(NULL), "Connection failed.  error %s\n", strerror(errno));
		return NULL;
	} else {
		// TODO:  Complete connection stuff.
	}
}