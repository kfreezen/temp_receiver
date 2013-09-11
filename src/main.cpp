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

#include "xbee.h"
#include "globaldef.h"
#include "serial.h"
#include "logger.h"
#include "SensorDB.h"

using namespace std;

FILE* __stdout_log;
char* log_buffer;

typedef struct {
	XBeeAddress addr;
	int lastPacketTime;
} Sensor;

// I feel dirty using these globals, but alas, I'm too lazy to do anything else.

map<XBeeAddress, Sensor*> sensorMap;
XBeeAddress receiver_addr;

// END GLOBALS

// SIGNAL STUFF
void basic_sig_handler(int);
void sighup_handler(int);

void sigint_handler(int sig) {
	fprintf(__stdout_log, "sigint.  sig=%d, exiting...\n", sig);
}

void sigquit_handler(int sig) {
	fprintf(__stdout_log, "sigquit.  sig=%d, exiting...\n", sig);
}

void sigill_handler(int sig) {
	fprintf(__stdout_log, "sigill.  sig=%d, exiting...\n", sig);
}

void sigabrt_handler(int sig) {
	fprintf(__stdout_log, "sigabrt.  sig=%d, exiting...\n", sig);
}

void sigfpe_handler(int sig) {
	fprintf(__stdout_log, "sigfpe.  sig=%d, exiting...\n", sig);
}

void sigkill_handler(int sig) {
	fprintf(__stdout_log, "sigkill.  sig=%d, exiting...\n", sig);
}

void sigsegv_handler(int sig) {
	fprintf(__stdout_log, "sigsegv.  sig=%d, exiting...\n", sig);
}

void sigpipe_handler(int sig) {
	fprintf(__stdout_log, "sigpipe.  sig=%d, exiting...\n", sig);
}

void sigalrm_handler(int sig) {
	fprintf(__stdout_log, "sigalrm.  sig=%d, exiting...\n", sig);
}

void sigterm_handler(int sig) {
	fprintf(__stdout_log, "sigterm.  sig=%d, exiting...\n", sig);
	exit(0);
}

void sigquit_action_handler(int sig, siginfo_t* siginfo, void* context) {
	fprintf(__stdout_log, "SIGQUIT received from %d, ignoring...\n", siginfo->si_pid);
}

void sigint_action_handler(int sig, siginfo_t* siginfo, void* context) {
	fprintf(__stdout_log, "SIGINT received from %d, ignoring...\n", siginfo->si_pid);
}

void init_sig_handlers() {
	signal(SIGHUP, sighup_handler);
	
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = sigint_action_handler;
	action.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &action, NULL);
	
	//signal(SIGINT, sigint_handler);

	//signal(SIGQUIT, sigquit_handler);
	action.sa_sigaction = sigquit_action_handler;
	sigaction(SIGQUIT, &action, NULL);

	signal(SIGILL, sigill_handler);
	signal(SIGABRT, sigabrt_handler);
	signal(SIGFPE, sigfpe_handler);
	signal(SIGKILL, sigkill_handler);
	signal(SIGSEGV, sigsegv_handler);
	signal(SIGPIPE, sigpipe_handler);
	signal(SIGALRM, sigalrm_handler);
	signal(SIGTERM, sigterm_handler);

}

void sighup_handler(int sig) {
	fprintf(__stdout_log, "sighup.  sig=%d, exiting...\n");
}

void basic_sig_handler(int sig) {
	fprintf(__stdout_log, "sig=%d, exiting...");
	exit(1);
}
// END SIGNAL STUFF

void AddSensor(XBeeAddress* addr);

string GetXBeeID(XBeeAddress* addr) {
	char* s = new char[17];
	int i;
	for(i=0; i<8; i++) {
		sprintf(&s[i<<1], "%02x", addr->addr[i]);
	}

	string _s = string(s);
	delete s;
	return _s;
}

void SensorUpdate(XBeeAddress* addr) {
	Sensor* sensor = sensorMap[*addr];
	if(sensor==NULL) {
		AddSensor(addr);
		sensor = sensorMap[*addr];
	}
	
	sensor->lastPacketTime = time(NULL);
}

void AddSensor(XBeeAddress* addr) {
	if(sensorMap[*addr]!=NULL) {
		return;
	}
	
	Sensor* sensor = new Sensor;
	memcpy(&sensor->addr, addr, sizeof(XBeeAddress));

	fprintf(__stdout_log, "id=%s\n", GetXBeeID(addr).c_str());
	
	SensorDB db;

	// Add the network if necessary.
	db.AddNetwork(GetXBeeID(&receiver_addr));
	
	// Add the sensor to the network DB.
	db.AddSensor(GetXBeeID(&receiver_addr), GetXBeeID(addr));

	sensorMap[*addr] = sensor;
}

void* sensor_scanning_thread(void* p) {
	while(1) {
		map<XBeeAddress, Sensor*>::iterator itr;
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

void SendPacket(SerialPort* port, XBeeAddress* address, Packet* packet) {
	XBAPI_Transmit(port, address, packet, sizeof(Packet));
}

void SendReceiverAddress(SerialPort* port, XBeeAddress* dest) {
	for(int i=0; i<sizeof(XBeeAddress); i++) {
		fprintf(__stdout_log, "%x%c", dest->addr[i], (i==sizeof(XBeeAddress)-1) ? '\n' : ' ');
	}

	Packet packet_buffer;
    packet_buffer.header.command = REQUEST_RECEIVER;
    packet_buffer.header.magic = 0xAA55;
    packet_buffer.header.revision = PROGRAM_REVISION;
    //packet_buffer.header.crc16 = CRC16_Generate((byte*)&packet_buffer, sizeof(Packet));
    // TODO:  Implement CRC16 generation.

    SendPacket(port, dest, &packet_buffer);
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

int main() {
	__stdout_log = fopen("stdout_log.log", "w+");
	fprintf(__stdout_log, "New receiver session at %lu\n", time(NULL));

	// Split this into a daemon.
	pid_t process_id = 0;
	pid_t sid = 0;
	
	fclose(__stdout_log);

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

	__stdout_log = fopen("stdout_log.log", "a+");
	setvbuf(__stdout_log, NULL, _IONBF, 0);

	sid = setsid();
	if(sid < 0) {
		fprintf(__stdout_log, "Failed getting session ID.\n");
		exit(1);
	}
	// Take care of logging the signals at least before we crash.	
	init_sig_handlers();
	
	// Now let's redirect stdout to a file called "receiver_stdout"
	FILE* fp = freopen("/tmp/temp_receiver_stdout", "a+", stdout);
	// Now since the above line didn't work for some reason...

	if(fp == NULL) {
		fprintf(__stdout_log, "Failure redirecting stdout.\n");
	}

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

	fprintf(__stdout_log, "We are go\n");

	pthread_t thread;
	//pthread_create(&thread, NULL, &sensor_scanning_thread, NULL);

	// TODO:  Add sensor config loading code here.

	// Get our Xbee ID.
	unsigned* buffer = new unsigned[16];
	XBAPI_Command(port, API_CMD_ATSL, &buffer[1], 1, 0);
	XBAPI_Command(port, API_CMD_ATSH, &buffer[0], 1, 0);
	buffer[0] = swap_endian_32(buffer[0]);
	buffer[1] = swap_endian_32(buffer[1]);
	
	memcpy(&receiver_addr, ((XBeeAddress*)buffer), sizeof(XBeeAddress));
	
	delete buffer;

	fprintf(__stdout_log, "We are starting the loop\n");

	while(1) {
		fflush(__stdout_log);
		XBAPI_HandleFrame(port, 0);
	
		//fprintf(__stdout_log, "while()\n");
		//port->read(buffer, 4);
		//fprintf(__stdout_log, "%x %x %x %x ", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
}
