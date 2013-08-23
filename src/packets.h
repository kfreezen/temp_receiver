#ifndef PACKETS_H
#define PACKETS_H

#define REPORT 0x00
#define CALIBRATE 0x01
#define REQUEST_RECEIVER 0x04

#include "globaldef.h"

// This is an application-defined, arbitrary format.

struct __PacketHeader {
    unsigned short magic; // 0xAA55
    unsigned short crc16;
    byte revision;
    byte command;
    unsigned short reserved;
} __attribute__((packed));

typedef struct __PacketHeader PacketHeader;

// This union allows me to select between using a byte buffer and a myriad of structures for my packets
typedef union {

	struct {
        PacketHeader header;

		union {

            struct {
                word thermistorResistance;
                word thermistorResistance25C;
                word thermistorBeta;
                word topResistorValue;
				byte xbee_reset;
            } report;

            struct {
                short thermistorResistance25CAdjust;
                short topResistorValueAdjust;
            } calibration;

            struct {
                word interval;
            } interval;

            struct {
                word networkID;
                byte preambleID;
            } set_id;

            struct {
                byte address[8];
            } receiver_address;
        } __attribute__((packed));
    } __attribute__((packed));

    byte packet_data[32];
} Packet;

#endif
