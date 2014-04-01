#ifndef PACKETS_H
#define PACKETS_H

#define TEMP_REPORT 0x00
#define REPORT 0x00

#define ERROR_REPORT 0x01
#define DIAG_REPORT 0x02

#define REQUEST_RECEIVER 0x04
#define RECEIVER_ACK 0x04
#define RANGE_TEST 0xFF

#include "globaldef.h"
#include "serial.h"
//#include "sensor.h"

// Should be in sensor.h, but due to circular dependencies,
// we must do this.
typedef union SensorId {
	byte id[8];
	#ifdef __GNUC__
	uint64 uId;
	#endif
} SensorId;

#define REVISION_0 0
#define REVISION_1 1

typedef struct __XBeeAddressInnerStruct {
    unsigned char addr[8];
} XBeeAddressInnerStruct;

typedef union __XBeeAddress {
	uint64 uAddr;
	unsigned char addr[8];
} XBeeAddress;

// This is an application-defined, arbitrary format.

struct __PacketHeaderRev0 {
    unsigned short magic; // 0xAA55
    unsigned short crc16;
    byte revision;
    byte command;
    unsigned short reserved;
} __attribute__((packed));

typedef struct __PacketHeaderRev0 PacketHeaderRev0;

struct __PacketHeaderRev1 {
	uint8 flags;
	uint8 reserved0;
	uint16 crc16;
	byte revision;
	byte command;
	uint16 reserved1;
	SensorId sensorId;
} __attribute__((packed));

typedef struct __PacketHeaderRev1 PacketHeaderRev1;

// This union allows me to select between using a byte buffer and a myriad of structures for my packets
typedef union __PacketRev0 {

	struct {
        PacketHeaderRev0 header;

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
				byte reserved[24];
			} requestReceiver;

			struct {
				XBeeAddress receiverAddr;
			} receiverAck;

        } __attribute__((packed));
    } __attribute__((packed));

    byte packet_data[32];
} PacketRev0;

#define NUM_PROBES 3

typedef union __PacketRev1 {

	struct {
        PacketHeaderRev1 header;

		union {

			struct {
				uint32 probeResistances[NUM_PROBES];
				uint32 probeResistance25C;
				uint16 probeBeta;
				uint16 batteryLevel;
				uint32 topResistorValue;
				uint16 macAckFails;
				uint16 networkAckFails;
				uint16 routeNotFoundFails;
				byte reserved1[2];
			} tempReport;

			struct {
                byte wdtResetSpot;
				byte reserved[31];
			} requestReceiver;

			struct {
				byte reserved[32];
			} receiverAck;

			struct {
				uint8 PORTA, PORTB, PORTC, PORTD, PORTE;
				uint8 TRISA, TRISB, TRISC, TRISD, TRISE;
				uint8 ANSELA, ANSELB, ANSELC, ANSELD, ANSELE;
				uint8 APFCON1;
				uint8 BAUD1CON;
				uint8 SP1BRGL, SP1BRGH;
				uint8 RC1STA, TX1STA;
				uint8 TMR1L, TMR1H;
				uint8 T1CON, T1GCON;
				uint8 OPTION_REG;
				uint8 INTCON;
				uint8 PIE1, PIR1;
				uint8 STATUS;
				uint8 reserved;
				uint8 reservedForExtendedDiagReport;
            } diagReport;

			struct {
				uint16 error;
				uint32 data;
			} errReport __attribute__((packed));

		} __attribute__((packed));
    } __attribute__((packed));

    byte packet_data[48];
} PacketRev1;

#define ERR_SUCCESS 0x0000
#define ADC_PVREF_TOO_LOW 0x0001
#define ADC_CONVERSION_TIMEOUT 0x0002
#define FVRRDY_TIMEOUT 0x0003
#define REQUEST_RECEIVER_TIMEOUT 0x0004

const char* GetErrorReportStr(int err);

// Flags
#define PROBE_SIDE_INDICATOR (1<<0)

void* sensor_scanning_thread(void* p);

#endif
