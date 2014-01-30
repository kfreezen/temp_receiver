#include <string>
#include <iostream>
#include <pthread.h>
#include <map>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>

#include "tests.h"
#include "xbee.h"
#include "globaldef.h"
#include "serial.h"
#include "logger.h"
#include "SensorDB.h"
#include "sensor.h"
#include "curl.h"
#include "settings.h"
#include "update.h"

using namespace std;

FILE* __stdout_log;
char* log_buffer;

// I feel dirty using these globals, but alas, I'm too lazy to do anything else.

XBeeAddress receiver_addr;
string receiverId;

int verbose = 0;

extern int packetsDebug;

// END GLOBALS

// SIGNAL STUFF

void sigterm_handler(int sig, siginfo_t* siginfo, void* context) {
	fprintf(__stdout_log, "sigterm.  from %d, sig=%d, exiting...\n", siginfo->si_pid, sig);
	
	XBeeCommunicator::cleanupDefault();

	exit(0);
}

void sigquit_action_handler(int sig, siginfo_t* siginfo, void* context) {
	fprintf(__stdout_log, "SIGQUIT received from %d, ignoring...\n", siginfo->si_pid);
}

void sigint_action_handler(int sig, siginfo_t* siginfo, void* context) {
	fprintf(__stdout_log, "SIGINT received from %d, exiting...\n", siginfo->si_pid);
	exit(0);
}

void init_sig_handlers() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = sigint_action_handler;
	action.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &action, NULL);
	
	action.sa_sigaction = sigquit_action_handler;
	sigaction(SIGQUIT, &action, NULL);
	
	action.sa_sigaction = sigterm_handler;
	sigaction(SIGTERM, &action, NULL);

}

// END SIGNAL STUFF

void* sensor_scanning_thread(void* p) {
	SensorMap sensorMap = GetSensorMap();
	while(1) {
		SensorMap::iterator itr;
		for(itr = sensorMap.begin(); itr != sensorMap.end(); itr++) {
			if(itr->second == NULL) {
				fprintf(__stdout_log, "Encountered NULL Sensor. %d\n", sensorMap.size());
				continue;
			}
			
			if(itr->second->lastPacketTime+90 < time(NULL) && itr->second->lastPacketTime) {
				cout<< "Sensor has gone more than 90 seconds without replying.\n";
			}
			
		}
		
		sleep(5);
	}
}

void hexdump(void* ptr, int len) {
	unsigned char* addr = (unsigned char*) ptr;
	int i;
	for(i=0; i<len; i++) {
		fprintf(__stdout_log, "%x%c", addr[i], (i%16) ? ' '  : '\n');
	}
}

extern unsigned swap_endian_32(unsigned n);

#define ENABLE_DAEMON 1
extern int xbeeDebug;
extern int noWeb;

// URGENT-TODO:  We do not have the xbee comm code completely revised yet.
int main(int argc, char** argv) {
	__stdout_log = stdout;
	setvbuf(__stdout_log, NULL, _IONBF, 0);
	
	int enableDaemon = ENABLE_DAEMON;

	argc--;
	argv++;
	for(int i=0; i < argc; i++) {
		if(!strcmp(argv[i], "tests")) {
			exit(tests());
		} else if(!strcmp(argv[i], "--daemon")) {
			enableDaemon = 1;
		} else if(!strcmp(argv[i], "--no-daemon") || !strcmp(argv[i], "-nd")) {
			enableDaemon = 0;
		} else if(!strcmp(argv[i], "--xbee-debug") || !strcmp(argv[i], "-dx")) {
			xbeeDebug = 1;
		} else if(!strcmp(argv[i], "--no-web") || !strcmp(argv[i], "-nw")) {
			noWeb = 1;
		} else if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			verbose = atoi(argv[++i]);
		} else if(!strcmp(argv[i], "--packets-debug") || !strcmp(argv[i], "-dp")) {
			packetsDebug = 1;
		}

	}

	if(enableDaemon == 1) {
		__stdout_log = fopen("stdout_log.log", "w+");
	}

	fprintf(__stdout_log, "New receiver session at %lu\n", time(NULL));

	// Split this into a daemon.
	pid_t process_id = 0;
	pid_t sid = 0;

	if(enableDaemon == 1) {
		fclose(__stdout_log);
	
		// forking!
		process_id = fork();

		if(process_id < 0) {
			printf("Creation of daemon failed.\n");
			exit(1);
		}

		if(process_id > 0) {
			printf("Child process PID = %d\n", process_id);
			exit(0);
		} else {
			//fclose(__stdout_log);
		}
		umask(0);
		// done forking!
	}

	__stdout_log = fopen("stdout_log.log", "a+");
	if(__stdout_log != NULL) {
		setvbuf(__stdout_log, NULL, _IONBF, 0);
	}
	
	if(enableDaemon == 1) {
		sid = setsid();
		if(sid < 0) {
			fprintf(__stdout_log, "Failed getting session ID.\n");
			exit(1);
		}
	}

	// Take care of logging the signals at least before we crash.	
	init_sig_handlers();
	
	FILE *fp, *errfp;

	// Now let's redirect stdout to a file called "receiver_stdout"
	if(enableDaemon == 1) {
		fp = freopen("/tmp/temp_receiver_stdout", "a+", stdout);
		errfp = freopen("/tmp/temp_receiver_stderr", "a+", stderr);
	} else {
		__stdout_log = stdout;
	}

	// Now since the above line didn't work for some reason...
	if(enableDaemon == 1) {
		if(fp == NULL) {
			fprintf(__stdout_log, "Failure redirecting stdout.\n");
		}
	}

	// Load our settings.
	loadSettings("receiver_settings");
	// loadSettings should probably be split off into its own file
	// eventually
	
	SerialPort* port = NULL;
	
	int portRetries = 60;
	while(1) {
		try {
			port = new SerialPort(9600);
		} catch(SerialPortException e) {
			// Didn't find a valid port, wait one second and try again.
			if(--portRetries) {
				sleep(1);
			} else {
				fprintf(__stdout_log, "We did not find a valid port.\n");
				return 1;
			}
		}

		if(port != NULL) {
			break;
		}
	}

	// Start our update checker.
	pthread_t updateCheckerThread;
	pthread_create(&updateCheckerThread, NULL, updateChecker, NULL);

	XBeeCommunicator::initDefault(port);
	XBeeCommunicator* comm = XBeeCommunicator::getDefault();
	comm->startDispatch();
	comm->startHandler();

	fprintf(__stdout_log, "We are go\n");

	pthread_t thread;
	//pthread_create(&thread, NULL, &sensor_scanning_thread, NULL);

	// TODO:  Add sensor config loading code here.

	// Get our Xbee ID.
	unsigned* buffer = new unsigned[8];
	int id0, id1;
	id0 = XBAPI_Command(comm, API_CMD_ATSL, NULL, 0);
	id1 = XBAPI_Command(comm, API_CMD_ATSH, NULL, 0);
	comm->waitCommStruct(id0);
	comm->waitCommStruct(id1);
	XBeeCommStruct* commStruct0 = comm->getCommStruct(id0);
	XBeeCommStruct* commStruct1 = comm->getCommStruct(id1);
	comm->freeCommStruct(id0);
	comm->freeCommStruct(id1);
	
	buffer[1] = swap_endian_32(*((unsigned*)commStruct0->replyData));
	buffer[0] = swap_endian_32(*((unsigned*)commStruct1->replyData));
	
	memcpy(&receiver_addr, ((XBeeAddress*)buffer), sizeof(XBeeAddress));
	
	delete buffer;

	// Load our application receiver ID.
	FILE* idFile = fopen("receiver.id", "r");
	if(idFile == NULL) {
		idFile = fopen("receiver.id", "w");

		SimpleCurl* _curl = new SimpleCurl();
		string url = Settings::get("server") + string("/api/getid");
		CURLBuffer* buf = _curl->get(url, "");
		
		if(buf == NULL) {
			fprintf(__stdout_log, "warning:  Something went wrong.  buf should not be null.\n");
		} else {
			fprintf(__stdout_log, "%s\n", buf->buffer);
			fwrite(buf->buffer, 1, strlen(buf->buffer), idFile);
			char _null = 0;
			fwrite(&_null, 1, 1, idFile);
			receiverId = string(buf->buffer);
		}
		
		fprintf(__stdout_log, "server=%s", url.c_str());
		
		if(buf != NULL) {
			delete buf;
		}

		delete _curl;
	} else {
		// Get length of file
		fseek(idFile, 0, SEEK_END);
		int end = ftell(idFile);
		fseek(idFile, 0, SEEK_SET);
		
		char* buf = new char [end + 1];
		
		memset(buf, 0, end + 1);
		
		fread(buf, 1, end, idFile);
		
		receiverId = string(buf);
		delete buf;
	}

	fclose(idFile);

	fprintf(__stdout_log, "We are starting the loop\n");

	while(1) {
		fflush(__stdout_log);
		sleep(1);
		//XBAPI_HandleFrame(port, 0);
	
		//fprintf(__stdout_log, "while()\n");
		//port->read(buffer, 4);
		//fprintf(__stdout_log, "%x %x %x %x ", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
