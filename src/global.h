#include <curses.h>

struct TConfig
{
	char Tracker[16];
	int EnableHabitat;
	int EnableSSDV;
	int LogLevel;
	double myLat, myLon, myAlt;

	WINDOW *Window;

	unsigned int TelemetryCount, SSDVCount, BadCRCCount, UnknownCount, SSDVMissing;

	char Payload[16], Time[12];
	unsigned int Counter;
	uint32_t Seconds;
	double Longitude, Latitude, Distance, Elevation;
	unsigned int Altitude, PreviousAltitude, Satellites;
	time_t LastPacketAt;
	int rssi, snr;
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
uint8_t   BattVoltage; // 0 = 0v, 255 = 5.0V, linear steps in-between.
uint16_t  Checksum; // CRC16-CCITT Checksum.
};
#pragma pack(pop)

#define PAYLOAD_COUNT 32
struct TPayload
{
        int InUse;
        char Payload[32];
};
