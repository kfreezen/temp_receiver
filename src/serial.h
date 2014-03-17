///
/// WARNING:  THIS HEADER FILE MAY CAUSE BRAIN DAMAGE IF YOU TRY TO UNDERSTAND IT'S CONTENTS.
/// IF YOU WISH TO REMAIN SANE, PLEASE STAY OUT!
///
/// EDIT:  It's a little more sane now that I don't use boost.

// (That's my way of knowing you I hate myself for writing such ugly code.)

#ifndef SERIAL_H
#define SERIAL_H

#include <string>
#include <vector>

#include <pthread.h>

#include <termios.h>

#include "globaldef.h"

using std::vector;
using std::string;

#define MAX_PORT_NUMBER 32
vector<string> findValidPorts(string portBase);

class SerialPortException {
	public:
		SerialPortException(string w) {
			what = w;
		}
		
		string what;
};

// TODO:  Add baud rate changing code.
class SerialPort {
	public:
		const static int DEFAULT_TIMEOUT_PER_CHAR = 10; // ms

		SerialPort(string port, int baud);
		SerialPort(int baud);
		
		~SerialPort();

		int reinit();
		
		bool readByte(unsigned char* p_c);

		// Also returns the amount of bytes read.
		int read(void* buffer, int len);
		void write(const void* buffer, int len);
		
		// Sets the time (in ns) that this object waits between chars. (Default is 1000/baud*25)
		void setCharTmo(unsigned int tmo) {
			charReadTimeout = (tmo<0) ? 0 : tmo;
			__timeout_struct.tv_nsec = charReadTimeout;
		}

		unsigned int getCharTmo() {
			return charReadTimeout;
		}

		// Attempts to reopen the serial port file.
		void heartbeat();

	private:
		void init(string port, int baud);
		void clearVars();
		
		//FILE* portFile;
		int portFileNo;

		// OK, Just to sort things out:
		// true means that the read timed out.
		// false means successful.
		bool readError;
		int numBytesRead;

		// Number of nanoseconds.
		unsigned int charReadTimeout;
		struct timespec __timeout_struct;
		
		int mBaud;

		// Heartbeat reconnection related members.
		uint64 lastHeartbeat;
		static const int HEARTBEAT_TIME = 30;

		// The purpose of this is to save the serial port's port options to reapply on heartbeat reconnections.
		struct termios portOptions;

		char* savedPortName;
};

#endif
