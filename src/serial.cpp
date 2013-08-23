#include "serial.h"

// This is the linux implementation
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

SerialPort::SerialPort(string port, int baud) {

	struct timeval timeout;
	int rv;
	SerialPort::portFile = fopen(port.c_str(), "r+");
	if(portFile==NULL) {
		printf("Portfileerror.\n");
	}
	setCharTmo(SerialPort::DEFAULT_TIMEOUT_PER_CHAR);


}

bool SerialPort::readByte(unsigned char* p_c) {
	ssize_t read_ret = ::fread(p_c, 1, 1, portFile);

	return true;
}

int SerialPort::read(void* buffer, int len) {
	fread(buffer, sizeof(char), len, portFile);
}

void SerialPort::write(void const* buffer, int len) {
	fwrite(buffer, sizeof(char), len, portFile);
}
