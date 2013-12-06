#ifndef XBEE_H
#define XBEE_H

#define API_AT_CMD_FRAME 0x8
#define API_TRANSMIT_FRAME 0x10

#define API_AT_CMD_RESPONSE 0x88
#define API_MODEM_STATUS 0x8A
#define API_TRANSMIT_STATUS 0x8B
#define API_RX_INDICATOR 0x90

#define API_CMD_ATSL 0x4C53
#define API_CMD_ATSH 0x4853

#include "packets.h"
#include "globaldef.h"
#include "serial.h"

bool operator<(const XBeeAddress& left, const XBeeAddress& right);

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
void XBAPI_Transmit(SerialPort* port, XBeeAddress* address, void* buffer, int length);
int XBAPI_Command(SerialPort* port, unsigned short command, unsigned* data, int id, int data_valid);

#endif
