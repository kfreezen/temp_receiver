#include <cstdio>
#include <cstring>

#include "xbee.h"
#include "serial.h"
#include "global_def.h"

#define START_DELIMITER 0x7e

extern void HandlePacket(SerialPort* port, Frame* apiFrame);

// This enables the map<...> to use the XBeeAddress as a key.
bool operator<(const XBeeAddress& left, const XBeeAddress& right) {
	if(sizeof(u64) == sizeof(XBeeAddress)) {
		u64 left_id = *((u64*)&left);
		u64 right_id = *((u64*)&right);
		return (left_id < right_id);
	} else {
		printf("sizeof(u64) != %d\n", sizeof(XBeeAddress));
		return false;
	}
}

word swap_endian_16(word n) {
	return ((n & 0xFF) << 8) | n>>8;
}

unsigned char checksum(void* addr, int length) {
    unsigned char* address = (unsigned char*) addr;

    // Calculate checksum
    unsigned char checksum = 0;
    int i;
    for(i=0; i<length; i++) {
        checksum += address[i];
    }

    return 0xFF - checksum;
}

unsigned char doChecksumVerify(unsigned char* address, int length, unsigned char checksum) {
    unsigned char check = 0;

    int i;
    for(i=0; i<length; i++) {
        check += address[i];
    }

    check += checksum;

    return check;
}

void XBAPI_Transmit(SerialPort* port, XBeeAddress* address, void* buffer, int length) {
	Frame apiFrame;

	apiFrame.tx.start_delimiter = 0x7e;
    apiFrame.tx.length[0] = (sizeof(TxFrame)-4) >> 8;
    apiFrame.tx.length[1] = (sizeof(TxFrame)-4) & 0xFF;
    apiFrame.tx.frame_id = 1; // TODO:  Add id param and frame handling.
    apiFrame.tx.frame_type = API_TRANSMIT_FRAME;
    memcpy(&apiFrame.tx.destination_address, address, sizeof(XBeeAddress));
    apiFrame.tx.reserved = 0xFEFF;
    apiFrame.tx.transmit_options = 0;
    apiFrame.tx.broadcast_radius = 0;
    memcpy(&apiFrame.tx.packet, buffer, (length>sizeof(Packet)) ? sizeof(Packet) : length);
    apiFrame.tx.checksum = checksum(apiFrame.buffer+3, sizeof(TxFrame)-4);

	port->write(&apiFrame, sizeof(TxFrame));

	XBAPI_HandleFrame(port, API_TRANSMIT_STATUS);
	
	printf("XBAPI_Transmit()\n");
}

#define READ_FRAME_NUM_RETRIES 2000
// Returns false if timeout after start delimiter found.
bool XBAPI_ReadFrame(SerialPort* port, Frame* frame) {
	unsigned char c = 0; // this byte is to check for two delimiter bytes.
	
	// Look for frame delimiter.
	int i;
	/*for(i=0; i<READ_FRAME_NUM_RETRIES; i++)*/
	while(1) {
		int bytesRead = port->read(&c, 1);
		// TODO:  Figure out what we want to do when a byte is not received.  For now just ignore it.
		if(c == START_DELIMITER && bytesRead>0) {
#ifdef DEBUG
			printf("Start delimiter found.\n");
#endif
			frame->buffer[0] = c;
			break;
		}
	}
	
	// Here we check to make sure that the xbee doesn't decide to send another start delimiter.
	//NOTE this assumes that length will not equal 0x007e or 0x7e00.

reset_for_loop: // I feel dirty doing this.  This is jumped to when we find another start delimiter while in the loop below.
	for(i=0; i<2; i++) {
		int bytesRead = port->read(&c, 1);
		if(c == START_DELIMITER) {
			// If c == START_DELIMITER we reset the length iterator.
#ifdef DEBUG
			printf("Another start delimiter found.\n");
#endif
			
			goto reset_for_loop; // The other way is to set i to -1, but I like using overflows to 0 in a loop less than I like using goto's.
			// XXX May not be the cleanest way to do this.
		} else if(bytesRead > 0) {
			frame->rx.length[i] = c;
		} else {
			return false;
		}
	}
	int length = (frame->rx.length[0] << 8) | frame->rx.length[1];
	
#ifdef DEBUG
	printf("length = %d, l[0]=%x, l[1]=%x\n", length, frame->rx.length[0], frame->rx.length[1]);
#endif

	// Read in the rest of the stuff.
	int bytesRead = port->read(frame->buffer+3, (length>sizeof(Frame)-3)?sizeof(Frame)-3:length); // That ()?: thingy is to prevent buffer overflows.

#ifdef DEBUG
	if(bytesRead < length) {
		FILE* dbg_out = fopen("debug_log.txt", "a");
		if(dbg_out==NULL) {
			printf("dbg_out ERROR.\n");
		}

		for(i=0; i<sizeof(Frame); i++) {
			fprintf(dbg_out, "%x ", frame->buffer[i]);
			printf("%x ", frame->buffer[i]);
		}

		fprintf(dbg_out, "\n");
		printf("\n");

		printf("Timed out.\n");
		fclose(dbg_out);
	} else {
		printf("Done with XBAPI_ReadFrame().\n");
	}
#endif

	return !(bytesRead < length);
}


extern void hexdump(void* ptr, int len);

u32 swap_endian_32(u32 n) {
    unsigned long r = 0;
    r |= (n&0xFF) << 24;
    r |= ((n>>8)&0xFF) << 16;
    r |= ((n>>16)&0xFF) << 8;
    r |= ((n>>24)&0xFF);

    return r;
}



int XBAPI_HandleFrame(SerialPort* port, int expected) {
	return XBAPI_HandleFrameEx(port, NULL, 0, expected);
}

int XBAPI_HandleFrameEx(SerialPort* port, void* data, int maxDataLength, int expected) {
	int returnValue = 0;
	
	Frame apiFrame;
	
	memset(&apiFrame, 0, sizeof(Frame));
	
	int length = XBAPI_ReadFrame(port, &apiFrame);
	
	if(length > sizeof(Frame)) {
		switch(expected) {
			case API_RX_INDICATOR:
				length = sizeof(RxFrame);
				//break;
			default:
				// Here we catch all the packet for examination.
				while(1) {
					unsigned char c;
					port->read(&c, 1);
					printf("%x ", c);
				}
				break;
		}
	}
	
	//port->read(apiFrame.buffer+3, length);
	
	printf("Handling ... ");
	
	switch(apiFrame.rx.frame_type) {
		case API_RX_INDICATOR: {
			HandlePacket(port, &apiFrame);
				
		} break;
		
		case API_AT_CMD_RESPONSE: {
			// First figure out the length of the data.
			// length of data is difference between api frame's length field and the sizeof(AtCmdResponse_NoData)-4
			int apiFrameLength = (apiFrame.rx.length[0] << 8) | apiFrame.rx.length[1];
			int dataLength = apiFrameLength - (sizeof(ATCmdResponse_NoData) - 4);
			// Now we take the data and put it into an unsigned int.
			if(dataLength>maxDataLength) {
				printf("dataLength==%d, maxDataLength==%d.  Returning.\n", dataLength, maxDataLength);
				return -1;
			}
			
			switch(dataLength) {
				case 0:
					break;
				case 1:
					*((unsigned char*)data) = apiFrame.atCmdResponse.data_checksum[0];
					break;
					
				case 2:
					*((unsigned char*)data+1) = apiFrame.atCmdResponse.data_checksum[0];
					*((unsigned char*)data) = apiFrame.atCmdResponse.data_checksum[1];
					break;
				case 4:
					*((unsigned*)data) = swap_endian_32(*((unsigned*)apiFrame.atCmdResponse.data_checksum));
					break;
					
				default:
					hexdump(&apiFrame, sizeof(ATCmdResponse));
					break;
			}
			
			returnValue = apiFrame.atCmdResponse.commandStatus;
			
			// TODO:  Finish.
		} break;
		
		default: {
			hexdump(&apiFrame, sizeof(apiFrame));
			break;
		}
	}
	
	printf("Returned\n");
	return returnValue;
}

int XBAPI_Command(SerialPort* port, unsigned short command, unsigned* data, int id, int data_valid) {
    
    /*int total_packet_length = 4 + length;

    apiFrame.buffer[0] = 0x7e;
    apiFrame.buffer[1] = total_packet_length >> 8;
    apiFrame.buffer[2] = total_packet_length & 0xFF;
    apiFrame.buffer[3] = API_AT_CMD_FRAME;
    apiFrame.buffer[4] = id;
    memcpy(&apiFrame.buffer[5], &command, 2);
    memcpy(&apiFrame.buffer[7], data, length);
    apiFrame.buffer[total_packet_length] = checksum(apiFrame.buffer+3, total_packet_length);
    */
    
    Frame apiFrame;
    
    byte atCmdLength = (data_valid) ? sizeof(ATCmdFrame) : sizeof(ATCmdFrame_NoData);

    apiFrame.atCmd.start_delimiter = 0x7e;
    apiFrame.atCmd.length[0] = (atCmdLength-4) >> 8;
    apiFrame.atCmd.length[1] = (atCmdLength-4) & 0xFF;
    apiFrame.atCmd.frame_type = API_AT_CMD_FRAME;
    apiFrame.atCmd.frame_id = id;
    apiFrame.atCmd.command = command;
    apiFrame.atCmd.data = swap_endian_32(*data);
   
    byte calc_checksum = checksum(apiFrame.buffer+3, atCmdLength-4);
    byte check = doChecksumVerify(apiFrame.buffer+3, atCmdLength-4, calc_checksum);
    
    if(data_valid) {
        apiFrame.atCmd.checksum = calc_checksum;
    } else {
        apiFrame.atCmdNoData.checksum = calc_checksum;
    }

    port->write(apiFrame.buffer, atCmdLength);

    if(id) {
        return XBAPI_HandleFrameEx(port, data, 4, API_AT_CMD_RESPONSE);
    } else {
        return 0;
    }
    
    // TODO:  Add checksum verification and a way to discard bytes that aren't in a frame.
}
