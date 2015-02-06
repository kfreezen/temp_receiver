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

#include "cJSON/cJSON.h"
#include "xbee.h"
#include "globaldef.h"
#include "serial.h"
#include "logger.h"
#include "SensorDB.h"
#include "sensor.h"
#include "curl.h"
#include "settings.h"
#include "update.h"
#include "test.h"
#include "packets.h"
#include "watchdog.h"
#include "ServerComm.h"

using namespace std;

char* log_buffer;

// I feel dirty using these globals, but alas, I'm too lazy to do anything else.

XBeeAddress receiver_addr;
uint64 receiverId;

int verbose = 0;

extern int packetsDebug;

// END GLOBALS

// SIGNAL STUFF

void exitCleanup() {
	XBeeCommunicator::cleanupDefault();
	watchdogQuit();
}

void sigterm_handler(int sig, siginfo_t* siginfo, void* context) {
	printf("sigterm.  from %d, sig=%d, exiting...\n", siginfo->si_pid, sig);
	
	// Prevent these signal handlers from executing twice.

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	action.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	exitCleanup();

	exit(0);
}

void sigquit_action_handler(int sig, siginfo_t* siginfo, void* context) {
	printf("SIGQUIT received from %d, ignoring...\n", siginfo->si_pid);
}

void sigint_action_handler(int sig, siginfo_t* siginfo, void* context) {
	printf("SIGINT received from %d, exiting...\n", siginfo->si_pid);

	// Prevent these signal handlers from executing twice.

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	action.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	exitCleanup();

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

void initReceiverId() {
	// Load our application receiver ID.
	FILE* idFile = fopen("receiver.id", "r");
	if(idFile != NULL) {
		fscanf(idFile, "%ld", &receiverId);

		if(receiverId == 0) {
			fclose(idFile);
			idFile = NULL;
		} else {
			return;
		}
	}

	if(idFile == NULL) {
		idFile = fopen("receiver.id", "w");

		SimpleCurl* _curl = new SimpleCurl();
		string url = Settings::get("server") + string("/api/id");

		std::vector<string> headers;
		headers.push_back("Accept-Version: 0.1");

		CURLBuffer* buf = _curl->get(url, "", headers);
		
		if(buf == NULL) {
			printf("Something went wrong.  buf should not be null.\n");
		} else {
			cJSON* json = cJSON_Parse(buf->buffer);
			if(!json) {
				printf("Something went wrong.  json should not be null. %s\n", buf->buffer);
			} else {
				cJSON* idItem = cJSON_GetObjectItem(json, "id");
				fprintf(idFile, "%ld", idItem->valueint);

				cJSON_Delete(json);
			}

			delete buf;
		}
		
		delete _curl;
	}

	fclose(idFile);
}

extern unsigned swap_endian_32(unsigned n);

#define ENABLE_DAEMON 1
extern int xbeeDebug;
extern int noWeb;
extern bool watchdogEnabled;
bool serverCommDisabled = false;

int main(int argc, char** argv) {
	// Not sure if this setvbuf is necessary anymore.
	setvbuf(stdout, NULL, _IONBF, 0);
	
	int enableDaemon = ENABLE_DAEMON;

	argc--;
	argv++;
	for(int i=0; i < argc; i++) {
		if(!strcmp(argv[i], "tests")) {
			doTests();
			exit(0);
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
		} else if(!strcmp(argv[i], "--no-wdt")) {
			watchdogEnabled = false;
		} else if(!strcmp(argv[i], "--no-servercomm")) {
			serverCommDisabled = true;
		}

	}

	printf("New receiver session at %lu\n", time(NULL));

	// Split this into a daemon.
	pid_t process_id = 0;
	pid_t sid = 0;

	if(enableDaemon == 1) {

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
		}
		umask(0);
		// done forking!
	}
	
	if(enableDaemon == 1) {
		sid = setsid();
		if(sid < 0) {
			printf("Failed getting session ID.\n");
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
	}

	// Now since the above line didn't work for some reason...
	if(enableDaemon == 1) {
		if(fp == NULL) {
			fprintf(stderr, "Failure redirecting stdout.\n");
		}

		if(errfp == NULL) {
			fprintf(stderr, "Failure redirecting stderr.\n");
		}
	}

	// Load our settings.
	loadSettings("receiver_settings");
	// loadSettings should probably be split off into its own file
	// eventually
	
	SerialPort* port = NULL;
	
	int maxPortRetries = 60;

	int portRetries = maxPortRetries;

	while(1) {
		try {
			port = new SerialPort(9600);
		} catch(SerialPortException e) {
			// Didn't find a valid port, wait one second and try again.
			if(--portRetries) {
				printf("Valid port not found. Trying again. %d retries left.\n", portRetries);
				sleep(1);
			} else {
				printf("We did not find a valid port.\n");
				// OK we want to wait 5 minutes, and try again.
				sleep(300);
				portRetries = maxPortRetries;
			}
		}

		if(port != NULL) {
			break;
		}
	}

	startWatchdogThread();

	// Start our update checker.
	// Currently disabled due to the unfinished state of the new API.
	/*pthread_t updateCheckerThread;
	pthread_create(&updateCheckerThread, NULL, updateChecker, NULL);*/

	XBeeCommunicator::initDefault(port);
	XBeeCommunicator* comm = XBeeCommunicator::getDefault();
	comm->startDispatch();
	comm->startHandler();

	printf("We are go\n");

	pthread_t thread;
	pthread_create(&thread, NULL, &sensor_scanning_thread, NULL);

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
	
	buffer[1] = swap_endian_32(*((unsigned*)commStruct0->replyData));
	buffer[0] = swap_endian_32(*((unsigned*)commStruct1->replyData));
	
	comm->freeCommStruct(id0);
	comm->freeCommStruct(id1);
	
	memcpy(&receiver_addr, ((XBeeAddress*)buffer), sizeof(XBeeAddress));
	
	delete[] buffer;
	
	initReceiverId();
	SensorDB sensorDB;
	sensorDB.AddReceiver(receiverId);

	ServerComm* serverComm = new ServerComm;
	
	if(!serverCommDisabled) {
		serverComm->start();
	}
		
	printf("We are starting the loop\n");

	while(1) {
		fflush(stdout);
		sleep(1);
		//XBAPI_HandleFrame(port, 0);
	
		//printf("while()\n");
		//port->read(buffer, 4);
		//printf("%x %x %x %x ", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
