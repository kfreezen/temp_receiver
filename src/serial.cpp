#include "serial.h"

// This is the linux implementation
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <vector>
#include <unistd.h>

using namespace std;

extern FILE* __stdout_log;

SerialPort::SerialPort(int baud) {
	string portFileBase = string("/dev/ttyUSB");
	vector<string> ports = findValidPorts(portFileBase);
	
	vector<string>::iterator port_itr;
	for(port_itr = ports.begin(); port_itr < ports.end(); port_itr++) {
		init(*port_itr, baud);
		return;
	}
	
	throw SerialPortException("No valid port found.");
}

SerialPort::SerialPort(string port, int baud) {
	init(port, baud);
}

void SerialPort::init(string port, int baud) {
	struct timeval timeout;
	int rv;
	SerialPort::portFile = fopen(port.c_str(), "r+");
	if(portFile==NULL) {
		throw SerialPortException("Port file error.");
	}
	setCharTmo(SerialPort::DEFAULT_TIMEOUT_PER_CHAR);
	mBaud = baud;
}

int SerialPort::reinit() {
	fprintf(__stdout_log, "Reconnecting...\n");
	
	string portFileBase = string("/dev/ttyUSB");
	vector<string> ports = findValidPorts(portFileBase);
	
	vector<string>::iterator port_itr;
	for(port_itr = ports.begin(); port_itr < ports.end(); port_itr++) {
	
		SerialPort::portFile = freopen((*port_itr).c_str(), "r+", SerialPort::portFile);
		if(portFile==NULL) {
			return -2;
		}
		init(*port_itr, mBaud);
		return 0;
	}
	
	return -1;
}

#define MAX_RECONNECT_RETRIES 32

bool SerialPort::readByte(unsigned char* p_c) {
	size_t read_ret = ::fread(p_c, 1, 1, portFile);
	if(read_ret != 1) {
		if(ferror(this->portFile)) {
			// Some error, let's try scanning and reinitializing on any serial port.
			
			int reconnectRetries = 0;
			while(SerialPort::reinit()==-1 && reconnectRetries++ < MAX_RECONNECT_RETRIES) {
				sleep(1);
			}
			
			if(reconnectRetries >= MAX_RECONNECT_RETRIES) {
				throw SerialPortException("Serial port reinit failed.");
			}
		}
			
	}
	
	return true;
}

int SerialPort::read(void* buffer, int len) {
	fread(buffer, sizeof(char), len, portFile);
}

void SerialPort::write(void const* buffer, int len) {
	fwrite(buffer, sizeof(char), len, portFile);
}

vector<string> findValidPorts(string portBase) {
	vector<string> retVector;
	
	FILE* port;
	char* buffer = new char[portBase.length() + 64];
	
	int i;
	for(i=0; i<32; i++) {
		sprintf(buffer, "%s%d", portBase.c_str(), i);
		port = fopen(buffer, "r");
		if(port != NULL) {
			retVector.push_back(string(buffer));
			fclose(port);
		}
	}
	
	return retVector;
}
