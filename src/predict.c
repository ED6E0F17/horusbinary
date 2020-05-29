/* Fake GPS generator and Gray code prediction */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

//void interleave(unsigned char *inout, int nbytes);
//void scramble(unsigned char *inout, int nbytes);
//int ldpc_encode_tx_packet(unsigned char *out, unsigned char *in);

/* Predicting Gray coded Payload data:
 *
 * The probability that any given bit will change is definable.
 *  ID will not change (for a single payload)
 *  Time increases slowly, and is mostly known
 *  Location changes slowly, and is mostly known
 *  Sensor data changes slowly unless broken
 *  Checksum is random
 *  - around 75% of the bits will not change
 *    A predictible 50% of the bits will not change with high probability
 *
 *  We cannot predict the checksum or the parity bits
 *
 *   As a first attempt, we mask off the top half of each byte
 *   and treat those bits as mostly invariant
 *   - modify the input by comparison with the history
 *
 *   (checking the per-bit consistancy from a typical flight would be instructive)
 */


// All data 8 bit Gray coded before calculating Checksum
//	- to improve soft bit prediction
struct __attribute__ ((packed)) BinaryPacket16
{
uint8_t   PayloadID;	// Legacy list
uint8_t   Counter;	// 8 bit counter
uint16_t  Biseconds;	// Time of day / 2
uint8_t   Latitude[3];	// (int)(float * 1.0e7) / (1<<8)
uint8_t   Longitude[3];	// ( better than 10m precision )
uint16_t  Altitude;	// 0 - 65 km
uint8_t   Voltage;	// scaled 5.0v in 255 range
uint8_t   User;		// Temp / Sats
	// Temperature	6 bits MSB => (+30 to -32)
	// Satellites	2 bits LSB => 0,4,8,12 is good enough
uint16_t  Checksum;	// CRC16-CCITT Checksum.
} FSK;	// 16 data bytes, for (128,384) LDPC FEC
	// (50 bytes at 100Hz 4fsk => 2 seconds)

#define PREDICTBYTES 14
int known[PREDICTBYTES] = {8, 8, 8,4, 8,8,3, 8,8,3, 8,3, 8, 8}; // Expected unchanged bits

void predict(float *softbits, uint8_t *last ) {
	int i, j;
	float data;
	float weight; // predicted data

	for (i=0; i < PREDICTBYTES ; i++) {
		for (j = 0; j < known[i]; j++) {
			data = 1.0f - 2.0f * last[i*8+j]; // bits are active low
			weight = (known[i] + 12 - j) * 0.3f;
			if ( softbits[i*8+j] * data > 0 )
				softbits[i*8+j] *= weight;
			else
				softbits[i*8+j] /= weight * 2;
		}
		// debug stuff here
	}
}

// Fake data
struct GPSdata {
	float Latitude;
	float Longitude;
	int32_t Altitude;
	uint8_t Satellites;
	int8_t  Temp;
	int16_t Speed;
	float Voltage;
} GPS;

static void makeGPS(uint16_t faketime) {
	float position;

	position = 51.0f + 0.5f * cosf((float)faketime/2000.0f);
	GPS.Latitude = position;
	position = 0.1f - 0.5f * sinf((float)faketime/3000.0f);
	GPS.Longitude = position;
	position = 500.0f +  200.0f * sinf((float)faketime/300.0f);
	GPS.Altitude = (int32_t)position;
	GPS.Satellites = (uint8_t)(position / 60.0f);
	GPS.Voltage = 3.5f - (float)faketime / (60.0f * 60 * 6);
	GPS.Temp = 30 - faketime / (60 * 6);
}

// pack position +/- 180.xx into 24 bits
// using Ublox 32bit binary format, truncated
static int32_t float_int32(float pos) {
	return (int32_t)(pos * 1.0e7f);
}

static void fill_FSK() {
	int32_t position, user, sats, temp, volts;
	static uint16_t counter = 0;

	FSK.PayloadID = 0;
	FSK.Counter = (uint8_t)counter++;
	FSK.Biseconds = (uint16_t)(counter * 11);

	makeGPS(counter * 22);
	position = float_int32(GPS.Longitude);
	FSK.Longitude[0] = 0xFF & (position >> 8);
	FSK.Longitude[1] = 0xFF & (position >>16);
	FSK.Longitude[2] = 0xFF & (position >>24);
	position = float_int32(GPS.Latitude);
	FSK.Latitude[0] = 0xFF & (position >> 8);
	FSK.Latitude[1] = 0xFF & (position >>16);
	FSK.Latitude[2] = 0xFF & (position >>24);
	FSK.Altitude = (uint16_t)GPS.Altitude;

	// (1.5v to 3.5v, for RS41) so scale 5.0V => 255
	volts = (int32_t)(GPS.Voltage * 51.0f);
	if (volts > 255) volts = 255;
	FSK.Voltage = (uint8_t)volts;

	// Six bit temperature, +31C to -32C in 1C steps
	temp = (int32_t)GPS.Temp;
	if (temp > 31) temp = 31;
	if (temp < -32) temp = -32;
	user = (uint8_t)(temp << 2);
	// 6 bits offset 2

	// rough guide to GPS quality, (0,4,8,12 sats)
	sats = GPS.Satellites >> 2;
	if (sats < 0) sats = 0;
	if (sats > 3) sats = 3;
	user |= (uint8_t)sats;
	// 2 bits offset 0

	FSK.User = user;
}

// UKHAS checksum calculator
static uint16_t array_CRC16_checksum(char *string, int len) {
  uint16_t crc = 0xffff;
  char i;
  int ptr = 0;
  while (ptr < len) {
    ptr++;
    crc = crc ^ (*(string++) << 8);
    for (i = 0; i < 8; i++) {
      if (crc & 0x8000)
        crc = (uint16_t) ((crc << 1) ^ 0x1021);
      else
        crc <<= 1;
    }
  }
  return crc;
}

static void arrayToGray(uint8_t *loc, uint8_t len)
{
	uint8_t i, n;
	uint8_t *ptr = loc;
	for (i = 0; i < len; i++) {
		n = *ptr;
		*ptr++ = n ^ (n>>1);
	}
}

// convert Bytes to Bits, MSB
uint8_t PayloadBits[sizeof(FSK) * 8];
uint8_t *getGPS(void) {
	uint8_t i, j;
	uint8_t *ptr;

	fill_FSK();
	arrayToGray((uint8_t*)&FSK, sizeof(FSK) - 2);
	FSK.Checksum = (uint16_t)array_CRC16_checksum((char*)&FSK, sizeof(FSK) - 2);
	// packet_length = ldpc_encode_tx_packet((uint8_t*)txbuff, (uint8_t*)&FSK);
	for(i=0; i<sizeof(PayloadBits); i++) {
		ptr = (uint8_t *)&FSK; 
		j = ptr[i >> 3];
		PayloadBits[i] = 1 & (j >> (7-i));
	}
	return PayloadBits;
}

#if 0
/*---------------------------------------------------------------------------*\

  Adapted from Horus 4fsk code in RS41HUP, licenced as GPL2:

  FILE........: horus_l2.c
  AUTHOR......: David Rowe
  DATE CREATED: Dec 2015

  Horus telemetry layer 2 processing.  Takes an array of 8 bit payload
  data, generates parity bits for error correction, interleaves
  data and parity bits, scrambles and prepends a Unique Word.

\*---------------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INTERLEAVER
#define SCRAMBLER
#define CODEBYTES (DATABYTES + PARITYBYTES)
//  Take payload data bytes, prepend a unique word and append parity bits
int ldpc_encode_tx_packet(unsigned char *out_data, unsigned char *in_data) {
    unsigned int   i, last = 0;
    unsigned char *pout;
    const char uw[] = { 0x1b, 0x1b,'$','$' };

    pout = out_data;
    memcpy(pout, uw, sizeof(uw));
    pout += sizeof(uw);
    memcpy(pout, in_data, DATA_BYTES);
    pout += DATA_BYTES;
    memset(pout, 0, PARITY_BYTES);

    // process parity bit offsets
    for (i = 0; i < NUMBERPARITYBITS; i++) {
        unsigned int shift, j;

	for(j = 0; j < MAX_ROW_WEIGHT; j++) {
		uint8_t tmp  = H_rows[i + j * NUMBERPARITYBITS] - 1;
		shift = 7 - (tmp & 7); // MSB
		last ^= in_data[tmp >> 3] >> shift;
	}
	shift = 7 - (i & 7); // MSB
	pout[i >> 3] |= (last & 1) << shift;
    }

    pout = out_data + sizeof(uw);
    interleave(pout, DATA_BYTES + PARITY_BYTES);
    scramble(pout, DATA_BYTES + PARITY_BYTES);

    return DATA_BYTES + PARITY_BYTES + sizeof(uw);
}

// single directional for encoding
void interleave(unsigned char *inout, int nbytes)
{
    uint16_t nbits = (uint16_t)nbytes*8;
    uint32_t i, j, ibit, ibyte, ishift, jbyte, jshift;
    unsigned char out[nbytes];

    memset(out, 0, nbytes);
    for(i=0; i<nbits; i++) {
        /*  "On the Analysis and Design of Good Algebraic Interleavers", Xie et al,eq (5) */
        j = (COPRIME * i) % nbits;
        
        /* read bit i  */
        ibyte = i>>3;
        ishift = i&7;
        ibit = (inout[ibyte] >> ishift) & 0x1;

	/* write bit i  to bit j position */ 
        jbyte = j>>3;
        jshift = j&7;
        out[jbyte] |= ibit << jshift; // replace with i-th bit
    }
 
    memcpy(inout, out, nbytes);
}

/* 16 bit DVB additive scrambler as per Wikpedia example */
void scramble(unsigned char *inout, int nbytes)
{
    int nbits = nbytes*8;
    int i, ibit, ibits, ibyte, ishift, mask;
    uint16_t scrambler = 0x4a80;  /* init additive scrambler at start of every frame */
    uint16_t scrambler_out;

    /* in place modification of each bit */
    for(i=0; i<nbits; i++) {

        scrambler_out = ((scrambler & 0x2) >> 1) ^ (scrambler & 0x1);

        /* modify i-th bit by xor-ing with scrambler output sequence */
        ibyte = i>>3;
        ishift = i&7;
        ibit = (inout[ibyte] >> ishift) & 0x1;
        ibits = ibit ^ scrambler_out;                  // xor ibit with scrambler output

        mask = 1 << ishift;
        inout[ibyte] &= ~mask;                  // clear i-th bit
        inout[ibyte] |= ibits << ishift;         // set to scrambled value

        /* update scrambler */
        scrambler >>= 1;
        scrambler |= scrambler_out << 14;
    }
}
#endif

