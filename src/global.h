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

//uint16_t  User[3];	// Available for use.
//uint32_t  NameID;	// six chars packed using SSDV method.
	// options for 32 byte extended packet

struct SBinaryPacket
{
	uint16_t BiSeconds;	//2,2
	uint8_t  Latitude[3];	//3,5
	uint8_t  Longitude[3];	//3,8
	uint16_t Altitude;	//2,10
	int16_t  User;		//2,12
	// Packed: 5bit ID | 4bit Volts*10 | 2bit Sats/4 | 5bit Temp/2
	uint16_t Checksum;	//2,14
};	// 14 byte compact packet
#pragma pack(pop)


