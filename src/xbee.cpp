#include <cstdio>
#include <cstring>
#include <bitset>
#include <cstddef>

#define XBEE_CPP

#include "xbee.h"
#include "serial.h"
#include "globaldef.h"
#include "packet_defs.h"

#define START_DELIMITER 0x7e

//#define XBEE_DEBUG

extern FILE* __stdout_log;

//extern void HandlePacket(SerialPort* port, Frame* apiFrame);

using std::bitset;
using std::deque;

const int XBeeCommunicator::MAX_CONCURRENT_COMMS = 255;

void XBeeCommunicator::initDefault(SerialPort* port) {
	XBeeCommunicator::defaultComm = new XBeeCommunicator(port);
}

XBeeCommunicator::XBeeCommunicator(SerialPort* port) {
	this->serialPort = port;
	this->xbeeCommBits = new vector<bool>(XBeeCommunicator::MAX_CONCURRENT_COMMS);
	this->xbeeComms = new XBeeCommStruct [MAX_CONCURRENT_COMMS + 1];
	memset(this->xbeeComms, 0, sizeof(XBeeCommStruct) * (MAX_CONCURRENT_COMMS+1));
}

void XBeeCommunicator::startHandler() {
	pthread_mutex_init(&this->handlerThreadMutex, NULL);
	pthread_cond_init(&this->handlerThreadCondition, NULL);

	int err = pthread_create(&this->handlerThread, NULL, (void*(*)(void*))XBeeCommunicator::handler, (void*) this);
	
	if(err) {
		string errReason;
		switch(err) {
			case EAGAIN:
				errReason = "EAGAIN:  Insufficient resources.";
				break;
			case EINVAL:
				errReason = "EINVAL:  Invalid settings in attr.";
				break;
			case EPERM:
				errReason = "EPERM:  No permision to allow settings specified in attr.";
				break;
		}

		fprintf(__stdout_log, "ERROR:  Failed to start XBee dispatcher thread.  reason: %s\n", errReason.c_str());
	}
}

void XBeeCommunicator::startDispatch() {
	pthread_mutex_init(&this->dispatchThreadMutex, NULL);
	pthread_cond_init(&this->dispatchThreadCondition, NULL);

	int err = pthread_create(&this->dispatchThread, NULL, (void*(*)(void*))XBeeCommunicator::dispatcher, (void*) this);

	if(err) {
		string errReason;
		switch(err) {
			case EAGAIN:
				errReason = "EAGAIN:  Insufficient resources.";
				break;
			case EINVAL:
				errReason = "EINVAL:  Invalid settings in attr.";
				break;
			case EPERM:
				errReason = "EPERM:  No permision to allow settings specified in attr.";
				break;
		}

		fprintf(__stdout_log, "ERROR:  Failed to start XBee dispatcher thread.  reason: %s\n", errReason.c_str());
	}

	// Now created, or should be anyway.
}

void XBeeCommunicator::registerRequest(XBeeCommRequest request) {
	
	pthread_mutex_lock(&this->dispatchThreadMutex);
	this->dispatchQueue.push_front(request);
	pthread_cond_signal(&this->dispatchThreadCondition);
	pthread_mutex_unlock(&this->dispatchThreadMutex);
}

void* XBeeCommunicator::handler(XBeeCommunicator* comm) {
	while(1) {
		// Read frame in.
		Frame* frame = new Frame;
		memset(frame, 0, sizeof(Frame));

		XBAPI_ReadFrame(comm->serialPort, frame);
		
		// Now, we need to determine which commStruct
		// that this one replied to.
		int commId = frame->txStatus.frame_id;
		// Give it its reply frame.
		comm->xbeeComms[commId-1].replyFrame = frame;
		XBeeCommStruct* commStruct = &comm->xbeeComms[commId-1];

		if(commStruct->callback == NULL) {
			commStruct->callback = XBAPI_HandleFrameCallback;
		}

		// Figure out which handler to call.
		/* Do it in this fashion:
			First call the user specified handler, if there is any.
			If there is none, or the handler does not handle it,
			handle it with the default handler.
		*/
		int cbRet = 0;
		if(commStruct->callback) {
			cbRet = commStruct->callback(commStruct);
		}

		if(cbRet == 0) {
			cbRet = XBAPI_HandleFrameCallback(commStruct);
		}
	}
}

void* XBeeCommunicator::dispatcher(XBeeCommunicator* comm) {
	// We basically need an infinite loop that dispatches all requests.

	while(1) {
		while(!comm->dispatchQueue.empty()) {
			XBeeCommRequest request = comm->dispatchQueue.back();
			comm->dispatchQueue.pop_back();

			// Here we need to find a free comm slot.
			int commId;
			int i;
			for(i = 0; i < XBeeCommunicator::MAX_CONCURRENT_COMMS; i++) {
				// Those convoluted dereference pointers make my head spin.
				bool xbeeCommBit = (*comm->xbeeCommBits)[i];
				if(xbeeCommBit == false) {
					(*comm->xbeeCommBits)[i] = true;
					break;
				}
			}

			commId = i + 1;
			comm->xbeeComms[commId-1].callback = request.callback;

			switch(request.commType) {
				case COMM_COMMAND: {
					unsigned short dest = *((unsigned short*)(&request.destination));

					XBAPI_CommandInternal(comm->serialPort, dest, (unsigned*) request.data, commId, (request.data==NULL || request.dataLength == 0) ? 0 : 1, &comm->xbeeComms[commId-1]);
				} break;

				case COMM_TRANSMIT:
					XBAPI_Transmit(comm->serialPort, (XBeeAddress*) request.destination, request.data, commId, request.dataLength, &comm->xbeeComms[commId-1]);
					break;
			}
		}

		pthread_mutex_lock(&comm->dispatchThreadMutex);
		int condRet = pthread_cond_wait(&comm->dispatchThreadCondition, &comm->dispatchThreadMutex);
		pthread_mutex_unlock(&comm->dispatchThreadMutex);
	}
}

void XBeeCommunicator::retry(XBeeCommStruct* commStruct) {
	if(commStruct->replyFrame != NULL) {
		delete commStruct->replyFrame;
		commStruct->replyFrame = NULL;
	}

	int length = 0;
	length |= (commStruct->origFrame->tx.rev0.length[0] << 8);
	length |= (commStruct->origFrame->tx.rev0.length[1]);
	commStruct->retries ++;
	this->serialPort->write(commStruct->origFrame, length + 4);
}

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

void XBAPI_Transmit(SerialPort* port, XBeeAddress* address, void* buffer, int id, int length, XBeeCommStruct* commStruct) {
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

	commStruct->origFrame = new Frame;
	memcpy(commStruct->origFrame, &apiFrame, sizeof(Frame));

	// The handler callbacks take care of this now.
	//XBAPI_HandleFrame(port, API_TRANSMIT_STATUS);

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

int XBAPI_HandleFrameCallback(XBeeCommunicator* comm, XBeeCommStruct* commStruct) {
	int returnValue = 0;
	
	Frame* apiFrame = commStruct->replyFrame;
	
	#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "Handling ... ");
	#endif

	switch(apiFrame->rx.rev0.frame_type) {
		case API_RX_INDICATOR: {
			// TODO:  We need to change this handle packet function to go through the XBeeCommunicator*
			HandlePacket(comm->getSerialPort(), apiFrame);
			return 1;
		}
		
		case API_AT_CMD_RESPONSE: {
			uint32* data = new uint32;
			uint32 maxDataLength = 4;

			// This default handler doesn't do anything with data received 
			// right now.

			// First figure out the length of the data.
			// length of data is difference between api frame's length field and the sizeof(AtCmdResponse_NoData)-4
			int apiFrameLength = (apiFrame->rx.rev0.length[0] << 8) | apiFrame->rx.rev0.length[1];
			int dataLength = apiFrameLength - (sizeof(ATCmdResponse_NoData) - 4);
			// Now we take the data and put it into an unsigned int.
			if(dataLength > maxDataLength) {
				fprintf(__stdout_log, "dataLength==%d, maxDataLength==%d.  Returning.\n", dataLength, maxDataLength);
				return -1;
			}
			
			switch(dataLength) {
				case 0:
					break;
				case 1:
					*((unsigned char*)data) = apiFrame->atCmdResponse.data_checksum[0];
					break;
					
				case 2:
					*((unsigned char*)data+1) = apiFrame->atCmdResponse.data_checksum[0];
					*((unsigned char*)data) = apiFrame->atCmdResponse.data_checksum[1];
					break;
				case 4:
					*((unsigned*)data) = swap_endian_32(*((unsigned*)apiFrame->atCmdResponse.data_checksum));
					break;
					
				default:
					#ifdef XBEE_DEBUG
					hexdump(&apiFrame, sizeof(ATCmdResponse));
					#endif
					
					break;
			}
			
			returnValue = apiFrame->atCmdResponse.commandStatus;
			
			// TODO:  Add retry code.
			return 1;
		} break;
		
		default: {
			#ifdef XBEE_DEBUG
			hexdump(&apiFrame, sizeof(apiFrame));
			#endif
			// TODO:  Add more status handlers.
			return 1;
		}
	}
	
	#ifdef XBEE_DEBUG
	fprintf(__stdout_log, "Returned\n");
	#endif
	
	return 1;
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

int XBAPI_Command(XBeeCommunicator* comm, unsigned short command, unsigned* data, int dataLength) {
	XBeeCommRequest request;
	request.callback = NULL;
	request.commType = COMM_COMMAND;
	request.destination = (void*) command;
	request.data = (void*) data;
	request.dataLength = dataLength;

	comm->registerRequest(request);
}

int XBAPI_CommandInternal(SerialPort* port, unsigned short command, unsigned* data, int id, int data_valid) {
    #ifdef XBEE_COMM_WORKING
    fprintf(__stdout_log, "XBAPI Command being issued.\n");
    #endif

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

#define MAX_TRANSMIT_RETRIES 3

int XBAPI_TransmitStatusHandler(XBeeCommunicator* comm, XBeeCommStruct* commStruct) {
	if(!commStruct->replyFrame) {
		return 0;
	}

	if(commStruct->replyFrame->txStatus.frame_type != API_TRANSMIT_STATUS) {
		return 0;
	}

	Frame* frame = commStruct->replyFrame;

	// Now let's determine whether we should retry this or not.
	// MAC ACK Failure, Network ACK Failure, Route Not Found are network problems, so
	// we should retry them at least 3 times.
	
	// Payload too large is an application problem, so info about it should be put
	// on stderr.
	switch(frame->txStatus.delivery_status) {
		case TRANSMIT_SUCCESS:
			return 1; // Successfully handled.

		case MAC_ACK_FAIL:
		case NETWORK_ACK_FAIL:
		case ROUTE_NOT_FOUND:
			if(commStruct->retries >= MAX_TRANSMIT_RETRIES) {
				return -1;
			} else {
				// Try again.
				comm->retry(commStruct);
			}

		case PAYLOAD_TOO_LARGE:
		case INDIRECT_MSG_UNREQUESTED:
			return -1;

		default:
			return 0;
	}
}