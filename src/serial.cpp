#include "serial.h"
#include "settings.h"

// This is the linux implementation
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <cstdint>
#include <termios.h>
#include <cstring>

using namespace std;

SerialPort::~SerialPort() {
	close(this->portFileNo);
	
	delete[] this->savedPortName;
}

SerialPort::SerialPort(int baud) {
	this->clearVars();

	this->mBaud = baud;

	if(this->findPortsAndInit() == -1) {
		throw SerialPortException("No valid port found.");
	} else {
		return;
	}
}

SerialPort::SerialPort(string port, int baud) {
	this->clearVars();

	init(port, baud);
}

// If baud is a non-standard rate, (e.g. 4000, 11000, 115300 and the like),
// this function defaults to 9600 baud.

int SerialPort::init(string port, int baud) {
	printf("SerialPort::init\n");
	
	if(this->savedPortName != NULL) {
		delete[] this->savedPortName;
	}

	this->portFileNo = open(port.c_str(), O_RDWR | O_NONBLOCK);
	if(this->portFileNo == -1) {
		// TODO:  Handle this error?  I can't really think of a reason why we 
		// would want to handle it.
		return -1;
	}

	if(!isatty(this->portFileNo)) {
		return -1;
	}

	this->lastHeartbeat = time(NULL);
	this->savedPortName = new char[strlen(port.c_str()) + 1];
	strcpy(this->savedPortName, port.c_str());

	// Here we want to set the attributes of our port as needed.
	struct termios portOpts;
	memset(&portOpts, 0, sizeof(struct termios));
	// Get the port attributes.
	if(tcgetattr(SerialPort::portFileNo, &portOpts) == -1) {
		printf("tcgetattr(%d, %p) error, errno=%d\n", SerialPort::portFileNo, &portOpts, errno);
		return -1; // TODO:  Implement error handling.
	}
	
	// I know we got the port options above, but we're going to clear
	// that struct now.
	memset(&portOpts, 0, sizeof(struct termios));
	
	portOpts.c_iflag = 0;
	portOpts.c_cflag = 0;
	portOpts.c_lflag = 0;
	
	// Translate our baud constant into something usable
	// by termios.
	int speedSelect = B9600; 
	
	switch(baud) {
		case 0: speedSelect = B0; break;
		case 50: speedSelect = B50; break;
		case 75: speedSelect = B75; break;
		case 110: speedSelect = B110; break;
		case 134: speedSelect = B134; break;
		case 150: speedSelect = B150; break;
		case 200: speedSelect = B200; break;
		case 300: speedSelect = B300; break;
		case 600: speedSelect = B600; break;
		case 1200: speedSelect = B1200; break;
		case 1800: speedSelect = B1800; break;
		case 2400: speedSelect = B2400; break;
		case 4800: speedSelect = B4800; break;
		case 9600: speedSelect = B9600; break;
		case 19200: speedSelect = B19200; break;
		case 38400: speedSelect = B38400; break;
		case 57600: speedSelect = B57600; break;
		case 115200: speedSelect = B115200; break;
		case 230400: speedSelect = B230400; break;
		default:
			baud = 9600;
			speedSelect = B9600;
			break;
	}
	
	// Now set our serial input and output baud.
	if(cfsetispeed(&portOpts, speedSelect)==-1) {
		printf("cfsetispeed(%p, %d) error, errno=%s\n", &portOpts, speedSelect, strerror(errno));
		return -1;
	}
	
	if(cfsetospeed(&portOpts, speedSelect)==-1) {
		printf("cfsetospeed(%p, %d) error, errno=%s\n", &portOpts, speedSelect, strerror(errno));
		return -1;
	}
	
	// Save the port options for heartbeats.
	SerialPort::portOptions = portOpts;

	// Write our modified options to the port's settings.
	if(tcsetattr(SerialPort::portFileNo, TCSANOW, &this->portOptions)==-1) {
		printf("tcsetattr(%d, %d, %p) error, errno=%s\n", SerialPort::portFileNo, TCSANOW, &this->portOptions, strerror(errno));
		if(errno == ENOTTY) {
			// Inappropriate ioctl for device.
			return -1; // TODO:  I think we should handle this, but up above we check to see if the fd is a tty.
		} else {
			return -1; // Something else went wrong.
			// Should we handle this differently?
		}
	}
	
	// Now flush it so we start fresh.
	tcflush(SerialPort::portFileNo, TCIOFLUSH);

	setCharTmo(1000000000 / baud);
	mBaud = baud;
}

int SerialPort::reinit() {
	printf("Reconnecting...\n");
	
	close(SerialPort::portFileNo);

	return this->findPortsAndInit();
}

int SerialPort::findPortsAndInit() {
	int baud = this->mBaud;

	// TODO:  Change this to a setting in the settings file.
	string portFileBase = string("/dev/ttyUSB");
	vector<string> ports = findValidPorts(portFileBase);

	vector<string>::iterator port_itr;
	for(port_itr = ports.begin(); port_itr < ports.end(); port_itr++) {
		int err = init(*port_itr, baud);
		if(err == 0) {
			return 0; // init was successful.
		} else if(err == -1) {
			continue; // device was not a tty, or tcsetattr failed with ENOTTY
			// We want to try the next device, if there was any.
		}
	}

	return -1;
}

#define MAX_RECONNECT_RETRIES 32

bool SerialPort::readByte(unsigned char* p_c) {
	size_t read_ret = -1;
	while(read_ret == -1) {
		read_ret = ::read(SerialPort::portFileNo, &p_c, 1);
		if(read_ret == -1) {
			// This is a nonblocking file, so
			// wait for enough time to receive the next character
			// then proceed.
			if(errno == EAGAIN) {
				struct timespec _sleep_time;
				_sleep_time.tv_nsec = SerialPort::charReadTimeout;
				_sleep_time.tv_sec = 0;
				nanosleep(&_sleep_time, NULL);
			} else {
				return false;
			}
		}
	}

	return true;
}

int SerialPort::read(void* buffer, int len) {
	int size = len;
	while(size > 0) {
		SerialPort::heartbeat();
		size_t amount_read = ::read(portFileNo, buffer, size);
	
		if(amount_read == -1) {
			// Handle error.
			printf("errno = %s\n", strerror(errno));
		} else if(amount_read < size) {
			int do_sleepwait = 0;
			if(amount_read < size && amount_read != -1) {
				// Probably because it's nonblocking.
				size -= amount_read;
				buffer = ((unsigned char*)buffer) + amount_read;
				do_sleepwait = 1;
			} else if(amount_read == -1) {
				if(errno == EINTR || errno == EAGAIN) {
					do_sleepwait = 2; 
				} else {
					break;
				}
			}

			if(do_sleepwait) {
				struct timespec _sleep_time;
				
				uint64_t time;
				if(do_sleepwait == 2) {
					// This means that there is nothing
					// in the buffer, so let's wait
					// 500 ms.
					time = 500L * 1000L * 1000L;
				} else {
					time = charReadTimeout * size;
				}
				_sleep_time.tv_nsec = time % 1000000000L;
				_sleep_time.tv_sec = time / 1000000000L;
				nanosleep(&_sleep_time, NULL);
			}

		} else {
			size -= amount_read;
		}

		//printf("while size=%d", size);
	}
	return len-size;
	//printf("return\n");
}

void SerialPort::write(void const* buffer, int len) {
	SerialPort::heartbeat();

	::write(portFileNo, buffer, len);
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
	
	delete[] buffer;

	return retVector;
}

void SerialPort::heartbeat() {
	if(time(NULL) >= SerialPort::lastHeartbeat + SerialPort::HEARTBEAT_TIME) {
		::close(SerialPort::portFileNo);
		
		while(1) {
			int retries = 3;

			while(retries --) {
				if(::open(SerialPort::savedPortName, O_RDWR | O_NONBLOCK) == -1) {
					fprintf(stderr, "heartbeat failed.  err %s\n", strerror(errno));
					// Wait for 30 seconds...
					sleep(30);
				} else {
					SerialPort::lastHeartbeat = time(NULL);

					// Set our saved port options, in case the port lost its settings.
					if(tcsetattr(this->portFileNo, TCSANOW, &this->portOptions)==-1) {
						printf("tcsetattr(%d, %d, %p) error, errno=%s\n", this->portFileNo, TCSANOW, &this->portOptions, strerror(errno));
						exit(1);
					}

					break;
				}
			}

			if(retries == 0) {
				// we need to take more drastic action.
				if(Settings::get("restart-on-failed-heartbeat") == "true") {
					// since the settings have specified restart on failed heartbeat,
					// we issue a reboot.

					system("reboot");
				} else {
					// There's not really anything to do but keep trying.
					retries = 5;
				}
			} else {
				break;
			}
		}
	}
}

void SerialPort::clearVars() {
	this->savedPortName = NULL;
}