#include <cstdio>
#include <cstring>

#include "xbee.h"
#include "serial.h"
#include "globaldef.h"
#include "packet_defs.h"

#define START_DELIMITER 0x7e

//#define XBEE_DEBUG

extern FILE* __stdout_log;

//extern void HandlePacket(SerialPort* port, Frame* apiFrame);

// This enables the map<...> to use the XBeeAddress as a key.
bool operator<(const XBeeAddress& left, const XBeeAddress& right) {
	if(sizeof(uint64) == sizeof(XBeeAddress)) {
		return left.uAddr < right.uAddr;
	} else {
		fprintf(__stdout_log, "sizeof(uint64) != %d\n", sizeof(XBeeAddress));
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

void XBAPI_Transmit(SerialPort* port, XBeeAddress* address, void* buffer, int id, int length) {
	Frame apiFrame;
	int size = 0;

	if(length == sizeof(PacketRev0)) {
		size = sizeof(TxFrameRev0);
		fprintf(__stdout_log, "TxFrameRev0\n");
	} else {
		size = sizeof(TxFrameRev1);
		fprintf(__stdout_log, "TxFrameRev1\n");
	}
	// We can use .tx.rev0 in these option settings because these are unchanged over revisions due to the fact that they
	// are part of the xbee's frames. 
	apiFrame.tx.rev0.start_delimiter = 0x7e;
	apiFrame.tx.rev0.length[0] = (size-4) >> 8;
	apiFrame.tx.rev0.length[1] = (size-4) & 0xFF;
	apiFrame.tx.rev0.frame_id = id; // TODO:  Add id param and frame handling.
	apiFrame.tx.rev0.frame_type = API_TRANSMIT_FRAME;
	memcpy(&apiFrame.tx.rev0.destination_address, address, sizeof(XBeeAddress));
	apiFrame.tx.rev0.reserved = 0xFEFF;
	apiFrame.tx.rev0.transmit_options = 0;
	apiFrame.tx.rev0.broadcast_radius = 0;

	// TODO:  We should truncate this depending on length, instead of having just one fixed length.

	memcpy(&apiFrame.tx.rev1.packet, buffer, (length>sizeof(PacketRev1)) ? sizeof(PacketRev1) : length);
	if(length == sizeof(PacketRev0)) {
		apiFrame.tx.rev0.checksum = checksum(apiFrame.buffer + 3, sizeof(TxFrameRev0)-4);
	} else {
		apiFrame.tx.rev1.checksum = checksum(apiFrame.buffer+3, sizeof(TxFrameRev1)-4);
	}

	port->write(&apiFrame, sizeof(TxFrameRev1));

	XBAPI_HandleFrame(port, API_TRANSMIT_STATUS);

	#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "XBAPI_Transmit()\n");
	#endif
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
#ifdef XBEE_DEBUG
			fprintf(__stdout_log, "Start delimiter found.\n");
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
#ifdef XBEE_DEBUG
			fprintf(__stdout_log, "Another start delimiter found.\n");
#endif
			
			goto reset_for_loop; // The other way is to set i to -1, but I like using overflows to 0 in a loop less than I like using goto's.
			// XXX May not be the cleanest way to do this.
		} else if(bytesRead > 0) {
			frame->rx.rev0.length[i] = c;
		} else {
			return false;
		}
	}
	int length = (frame->rx.rev0.length[0] << 8) | frame->rx.rev0.length[1];
	length += 1; // This is so that we include the checksum as well.

#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "length = %d, l[0]=%x, l[1]=%x\n", length, frame->rx.rev0.length[0], frame->rx.rev0.length[1]);
#endif

	// Read in the rest of the stuff.
	int bytesRead = port->read(frame->buffer+3, (length>sizeof(Frame)-3)?sizeof(Frame)-3:length); // That ()?: thingy is to prevent buffer overflows.

#ifdef XBEE_DEBUG
	if(bytesRead < length) {
		FILE* dbg_out = fopen("debug_log.txt", "a");
		if(dbg_out==NULL) {
			fprintf(__stdout_log, "dbg_out ERROR.\n");
		}
		
		for(i=0; i<sizeof(Frame); i++) {
			fprintf(dbg_out, "%x ", frame->buffer[i]);
			fprintf(__stdout_log, "%x ", frame->buffer[i]);
		}

		fprintf(dbg_out, "\n");
		fprintf(__stdout_log, "\n");

		fprintf(__stdout_log, "Timed out.\n");
		fclose(dbg_out);
	} else {
		fprintf(__stdout_log, "Done with XBAPI_ReadFrame().\n");
	}
#endif

	return !(bytesRead < length);
}

#ifdef XBEE_DEBUG
extern void hexdump(void* ptr, int len);
#endif

uint32 swap_endian_32(uint32 n) {
    uint32 r = 0;
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
	
	/*if(length > sizeof(Frame)) {
		switch(expected) {
			case API_RX_INDICATOR:
				length = sizeof(RxFrame);
				//break;
			default:
				// Here we catch all the packet for examination.
				while(1) {
					unsigned char c;
					port->read(&c, 1);
					
					fprintf(__stdout_log, "%x ", c);
					
				}
				break;
		}
	}*/
	
	//port->read(apiFrame.buffer+3, length);
	
	#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "Handling ... ");
	#endif
	
	switch(apiFrame.rx.rev0.frame_type) {
		case API_RX_INDICATOR: {
			HandlePacket(port, &apiFrame);
				
		} break;
		
		case API_AT_CMD_RESPONSE: {
			// First figure out the length of the data.
			// length of data is difference between api frame's length field and the sizeof(AtCmdResponse_NoData)-4
			int apiFrameLength = (apiFrame.rx.rev0.length[0] << 8) | apiFrame.rx.rev0.length[1];
			int dataLength = apiFrameLength - (sizeof(ATCmdResponse_NoData) - 4);
			// Now we take the data and put it into an unsigned int.
			if(dataLength>maxDataLength) {
				fprintf(__stdout_log, "dataLength==%d, maxDataLength==%d.  Returning.\n", dataLength, maxDataLength);
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
					#ifdef XBEE_DEBUG
					hexdump(&apiFrame, sizeof(ATCmdResponse));
					#endif
					
					break;
			}
			
			returnValue = apiFrame.atCmdResponse.commandStatus;
			
			// TODO:  Finish.
		} break;
		
		default: {
			#ifdef XBEE_DEBUG
			hexdump(&apiFrame, sizeof(apiFrame));
			#endif
			break;
		}
	}
	
	#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "Returned\n");
	#endif
	
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
