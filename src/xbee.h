#ifndef XBEE_H
#define XBEE_H

#include <deque>

#include "packets.h"
#include "globaldef.h"
#include "serial.h"

#define API_AT_CMD_FRAME 0x8
#define API_TRANSMIT_FRAME 0x10

#define API_AT_CMD_RESPONSE 0x88
#define API_MODEM_STATUS 0x8A
#define API_TRANSMIT_STATUS 0x8B
#define API_RX_INDICATOR 0x90

#define COMM_COMMAND API_AT_CMD_FRAME
#define COMM_TRANSMIT API_TRANSMIT_FRAME

#define API_CMD_ATSL 0x4C53
#define API_CMD_ATSH 0x4853

#define TRANSMIT_SUCCESS 0x00
#define MAC_ACK_FAIL 0x01
#define NETWORK_ACK_FAIL 0x02
#define ROUTE_NOT_FOUND 0x25
#define PAYLOAD_TOO_LARGE 0x74
#define INDIRECT_MSG_UNREQUESTED 0x75

using namespace std;

bool operator<(const XBeeAddress& left, const XBeeAddress& right);

// Returns 1 if handled, 0 if not handled, -1 if error.

typedef struct __XBeeCommStruct XBeeCommStruct;
union __Frame;

class XBeeCommunicator;

typedef int (*XBeeHandleCallback)(XBeeCommunicator* comm, XBeeCommStruct* commStruct);

struct __XBeeCommStruct {
	XBeeHandleCallback callback;
	union __Frame* origFrame;
	union __Frame* replyFrame;
	int retries;
};

typedef struct {
	XBeeHandleCallback callback; // the handler will use this to do the lower-level work of handling, and determining what to do after a failure.

	int commType; // uses the API definitions for frame types.
	
	void* destination; // Meaning varies depending on commType
	void* data; // Is interpreted multiple ways, based on the communication type.  NULL means no data.
	int dataLength; // Length of the data.  0 means no data.
} XBeeCommRequest;

class XBeeCommunicator {
public:
	const static int MAX_CONCURRENT_COMMS;

	XBeeCommunicator(SerialPort* port);
	~XBeeCommunicator();

	// Starts dispatcher thread.
	void startDispatch();

	void startHandler();

	void registerRequest(XBeeCommRequest request);

	void retry(XBeeCommStruct* commStruct);

	void freeCommStruct(int id);

	SerialPort* getSerialPort() {
		return this->serialPort;
	}

	static void initDefault(SerialPort* port);
	static XBeeCommunicator* getDefault() {
		return XBeeCommunicator::defaultComm;
	}
private:
	static XBeeCommunicator* defaultComm;

	vector<bool>* xbeeCommBits;
	XBeeCommStruct* xbeeComms;

	deque<XBeeCommRequest> dispatchQueue;
	
	pthread_t dispatchThread;
	pthread_mutex_t dispatchThreadMutex;
	pthread_cond_t dispatchThreadCondition;

	pthread_t handlerThread;
	pthread_mutex_t handlerThreadMutex;
	pthread_cond_t handlerThreadCondition;

	SerialPort* serialPort;

	// These two are called by C functions (pthread), so they need to be static class methods.
	static void* dispatcher(XBeeCommunicator* comm);
	static void* handler(XBeeCommunicator* comm);
};

typedef struct __XBeeAddress_7Bytes XBeeAddress_7Bytes;

struct __ATCmdFrame {
    byte start_delimiter;
    byte length[2];
    byte frame_type;
    byte frame_id;

    union {
        unsigned short command;
        byte command_bytes[2];
    };

    unsigned data;
    byte checksum;
} __attribute__((packed));

struct __ATCmdFrame_NoData {
    byte start_delimiter;
    byte length[2];
    byte frame_type;
    byte frame_id;

    union {
        unsigned short command;
        byte command_bytes[2];
    };

    byte checksum;
} __attribute__((packed));


struct __ATCmdResponse {
	byte start_delimiter;
	byte length[2];
	byte frame_type;
	byte frame_id;
	
	union {
		unsigned short command;
		byte command_bytes[2];
	};
	
	byte commandStatus;
	
	byte data_checksum[5]; // This is data with checksum since the data may be variable length
} __attribute__((packed));

struct __ATCmdResponse_NoData {
	byte start_delimiter;
	byte length[2];
	byte frame_type;
	byte frame_id;
	
	union {
		unsigned short command;
		byte command_bytes[2];
	};
	
	byte commandStatus;
	
	byte checksum;
} __attribute__((packed));

struct __RxFrameRev0 {
	byte start_delimiter;
	byte length[2]; // Big-endian length of frame not counting first three bytes or checksum
	byte frame_type;
	byte frame_id;
	XBeeAddress_7Bytes source_address;
	unsigned short reserved; // Should equal 0xFEFF
	byte receive_options;
	PacketRev0 packet;
	byte checksum;
} __attribute__((packed));

struct __RxFrameRev1 {
    byte start_delimiter;
	byte length[2]; // Big-endian length of frame not counting first three bytes or checksum
	byte frame_type;
	byte frame_id;
	XBeeAddress_7Bytes source_address;
	unsigned short reserved; // Should equal 0xFEFF
	byte receive_options;
	PacketRev1 packet;
	byte checksum;
} __attribute__((packed));

struct __TxFrameRev0 {
    byte start_delimiter;
    byte length[2]; // Big-endian length of frame not counting first three bytes or checksum
    byte frame_type;
    byte frame_id;
    XBeeAddress destination_address;
    unsigned short reserved; // Should equal 0xFEFF
    byte broadcast_radius;
    byte transmit_options;
    PacketRev0 packet;
    byte checksum;
} __attribute__((packed));

struct __TxFrameRev1 {
    byte start_delimiter;
    byte length[2]; // Big-endian length of frame not counting first three bytes or checksum
    byte frame_type;
    byte frame_id;
    XBeeAddress destination_address;
    unsigned short reserved; // Should equal 0xFEFF
    byte broadcast_radius;
    byte transmit_options;
    PacketRev1 packet;
    byte checksum;
} __attribute__((packed));

struct __TxStatusFrame {
    byte start_delimiter;
    byte length[2]; // Big-endian length of frame not counting first three bytes or checksum
    byte frame_type;
    byte frame_id;
    unsigned short reserved; // Should equal 0xFEFF
    byte transmit_retry_count;
    byte delivery_status;
    byte discovery_status;
    byte checksum;
} __attribute__((packed));

typedef struct __ATCmdFrame ATCmdFrame;
typedef struct __ATCmdFrame_NoData ATCmdFrame_NoData;

typedef struct __RxFrameRev0 RxFrameRev0;
typedef struct __RxFrameRev1 RxFrameRev1;

typedef struct __TxFrameRev0 TxFrameRev0;
typedef struct __TxFrameRev1 TxFrameRev1;

typedef struct __TxStatusFrame TxStatusFrame;
typedef struct __ATCmdResponse ATCmdResponse;
typedef struct __ATCmdResponse_NoData ATCmdResponse_NoData;

typedef union __Frame {
    
	union {
		RxFrameRev0 rev0;
		RxFrameRev1 rev1;
	} rx;

	union {
		TxFrameRev0 rev0;
		TxFrameRev1 rev1;
	} tx;

	TxStatusFrame txStatus;

	ATCmdFrame atCmd;
	ATCmdFrame_NoData atCmdNoData;

	ATCmdResponse atCmdResponse;
	ATCmdResponse_NoData atCmdResponseNoData;

	byte buffer[128];

} Frame;

int XBAPI_HandleFrame(SerialPort* port, int expected);
int XBAPI_HandleFrameEx(SerialPort* port, void* data, int length, int expected);

unsigned char checksum(void* addr, int length);
unsigned char doChecksumVerify(unsigned char* address, int length, unsigned char checksum);
void XBAPI_Transmit(SerialPort* port, XBeeAddress* address, void* buffer, int id, int length, XBeeCommStruct* comm = NULL);

int XBAPI_Command(XBeeCommunicator* comm, unsigned short command, unsigned* data, int dataLength);
int XBAPI_CommandInternal(SerialPort* port, unsigned short command, unsigned* data, int id, int dataLength, XBeeCommStruct* comm = NULL);


void XBee_RegisterCommRequest(XBeeCommRequest request);

bool XBAPI_ReadFrame(SerialPort* port, Frame* frame);

int XBAPI_HandleFrameCallback(XBeeCommunicator* comm, XBeeCommStruct* commStruct);

// We want to take references to XBAPI_HandleFrame and XBAPI_HandleFrameEx out of external code.
// because we want the code to take better care of itself.  It should use callbacks instead of freezing
// for transmit statuses, etc.

#ifndef XBEE_CPP
#pragma GCC poison XBAPI_HandleFrame
#pragma GCC poison XBAPI_HandleFrameEx
#endif

#endif
