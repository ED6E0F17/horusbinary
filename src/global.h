#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>

#include "horus_api.h"
#include "fsk.h"
#include "horus_l2.h"
#include "modem_stats.h"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>
#include <curses.h>
#include <math.h>

#include "hiperfifo.h"
#include "utils.h"

#define WATERFALL_SHOW  40 /* chars to display */
#define WATERFALL_SIZE  64 /* size of buffer   */
#define PAYLOAD_COUNT 32
#define PAYLOAD_SIZE  16

struct TConfig
{
	char Tracker[16];
	int EnableHabitat, EnableSSDV, Mode;
	double myLat, myLon, myAlt;

	WINDOW *Window;

	unsigned int BinaryCount, LDPCCount, RTTYCount;
	unsigned int BadCRCCount, UnknownCount;
	char Payload[PAYLOAD_SIZE], Time[12];
	uint32_t Counter, Seconds;
	double Longitude, Latitude, Distance, Elevation;
	unsigned int Altitude, PreviousAltitude, Satellites;
	time_t LastPacketAt;
	int rssi, snr, ppm, freq;
	char Payloads[PAYLOAD_COUNT][PAYLOAD_SIZE];
	char Waterfall[WATERFALL_SIZE];
};
extern struct TConfig Config;

#pragma pack(push,1) 
struct TBinaryPacket
{
uint8_t   PayloadID;
uint16_t  Counter;
uint8_t   Hours;
uint8_t   Minutes;
uint8_t   Seconds;
union { float f; int32_t i; } Latitude;
union { float f; int32_t i; } Longitude;
uint16_t  Altitude;
uint8_t   Speed;
uint8_t   Sats;
int8_t    Temp;
uint8_t   BattVoltage;	// 0 = 0v, 255 = 5.0V, linear steps in-between.
uint16_t  Checksum;	// Legacy CRC16-CCITT Checksum.
};	// 22 byte legacy packet

struct SBinaryPacket
/* Short binary packet */
// 4 byte preamble for high error rates ("ESC,ESC,$,$")
// All data 8 bit Gray coded up to Checksum
//	- to improve soft bit prediction
{
uint8_t   PayloadID;	// Legacy list
uint8_t   Counter;	// 8 bit counter
uint16_t  BiSeconds;	// Time of day / 2
uint8_t   Latitude[3];	// (int)(float * 1.0e7) / (1<<8)
uint8_t   Longitude[3];	// ( better than 10m precision )
uint16_t  Altitude;	// 0 - 65 km
uint8_t   Voltage;	// scaled 5.0v in 255 range
uint8_t   User;		// Temp / Sats
	// Temperature	6 bits MSB => (+30 to -32)
	// Satellites	2 bits LSB => 0,4,8,12 is good enough
uint16_t  Checksum;	// CRC16-CCITT Checksum.
};	// 16 data bytes, for (128,384) LDPC FEC
	// => 52 bytes at 100Hz 4fsk => 2 seconds
#pragma pack(pop)


