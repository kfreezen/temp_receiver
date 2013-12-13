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

using namespace std;

FILE* __stdout_log;
char* log_buffer;

// I feel dirty using these globals, but alas, I'm too lazy to do anything else.

XBeeAddress receiver_addr;

// END GLOBALS

// SIGNAL STUFF

void sigterm_handler(int sig) {
	fprintf(__stdout_log, "sigterm.  sig=%d, exiting...\n", sig);
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

	signal(SIGTERM, sigterm_handler);

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

XBeeAddress TransformTo8ByteAddress(XBeeAddress_7Bytes address_7bytes) {
	XBeeAddress address;

	// Do transformation.
	memset(&address, 0, sizeof(XBeeAddress));

	// Copy it over.  Maybe we should have some special code in case the sizes change in the future or something.  Oh well, it's not like it's gonna change very soon.
	memcpy(&address.addr[1], &address_7bytes, sizeof(XBeeAddress_7Bytes));

	return address;
}

extern unsigned swap_endian_32(unsigned n);

#define ENABLE_DAEMON 0

// URGENT-TODO:  We do not have the xbee comm code completely revised yet.
int main(int argc, char** argv) {
	__stdout_log = stdout;
	setvbuf(__stdout_log, NULL, _IONBF, 0);
	
	argc--;
	argv++;
	for(int i=0; i < argc; i++) {
		if(!strcmp(argv[i], "tests")) {
			exit(tests());
		}
	}

#if ENABLE_DAEMON == 1
	__stdout_log = fopen("stdout_log.log", "w+");
#endif

	fprintf(__stdout_log, "New receiver session at %lu\n", time(NULL));

	// Split this into a daemon.
	pid_t process_id = 0;
	pid_t sid = 0;

#if ENABLE_DAEMON == 1
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
#endif

	__stdout_log = fopen("stdout_log.log", "a+");
	setvbuf(__stdout_log, NULL, _IONBF, 0);

#if ENABLE_DAEMON == 1
	sid = setsid();
	if(sid < 0) {
		fprintf(__stdout_log, "Failed getting session ID.\n");
		exit(1);
	}
#endif

	// Take care of logging the signals at least before we crash.	
	init_sig_handlers();
	
	// Now let's redirect stdout to a file called "receiver_stdout"
#if ENABLE_DAEMON == 1
	FILE* fp = freopen("/tmp/temp_receiver_stdout", "a+", stdout);
	FILE* errfp = freopen("/tmp/temp_receiver_stderr", "a+", stderr);
#else
	__stdout_log = stdout;
#endif

	// Now since the above line didn't work for some reason...
#if ENABLE_DAEMON == 1
	if(fp == NULL) {
		fprintf(__stdout_log, "Failure redirecting stdout.\n");
	}
#endif

	// Load our settings.
	loadSettings("receiver_settings");
	// loadSettings should probably be split off into its own file
	// eventually
	
	SerialPort* port = NULL;

	try {
		port = new SerialPort(9600);
	} catch(SerialPortException e) {
		fprintf(__stdout_log, "We did not find a valid port.\n");
		return 1;
	}

	XBeeCommunicator::initDefault(port);
	XBeeCommunicator* comm = XBeeCommunicator::getDefault();
	comm->startDispatch();
	comm->startHandler();

	fprintf(__stdout_log, "We are go\n");

	pthread_t thread;
	//pthread_create(&thread, NULL, &sensor_scanning_thread, NULL);

	// TODO:  Add sensor config loading code here.

	// Get our Xbee ID.
	unsigned* buffer = new unsigned[16];
	XBAPI_Command(comm, API_CMD_ATSL, &buffer[1], sizeof(unsigned));
	XBAPI_Command(comm, API_CMD_ATSH, &buffer[0], sizeof(unsigned));
	buffer[0] = swap_endian_32(buffer[0]);
	buffer[1] = swap_endian_32(buffer[1]);
	
	memcpy(&receiver_addr, ((XBeeAddress*)buffer), sizeof(XBeeAddress));
	
	delete buffer;

	fprintf(__stdout_log, "We are starting the loop\n");

	while(1) {
		fflush(__stdout_log);
		//XBAPI_HandleFrame(port, 0);
	
		//fprintf(__stdout_log, "while()\n");
		//port->read(buffer, 4);
		//fprintf(__stdout_log, "%x %x %x %x ", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
