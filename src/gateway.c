/*
  Copyright (C) 2018 David Rowe
	    (C) 2016 Dave Ake / UKHAS
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "global.h"

struct TConfig Config;
struct TPayload Payloads[PAYLOAD_COUNT];
struct   horus *hstates;
int audio_input_iq = 0;
int max_demod_in = 0;

/* It would be possible to run a single input through the decoder at two speeds */
int horus_init( int mode ) {
	hstates = horus_open( mode ? HORUS_MODE_SSDV : HORUS_MODE_BINARY );
	if ( hstates == NULL ) {
		fprintf( stderr, "Couldn't open Horus API\n" );
		return 0;
	}
	max_demod_in = horus_get_max_demod_in( hstates );
	return 1;
}

void horus_exit( void ) {
	horus_close( hstates );
}

/* Converts a hex character to its integer value */
char fromhex( char ch ) {
	return 0x0f & ( isdigit( ch ) ? ch - '0' : tolower( ch ) - 'a' + 10 );
}

/* why ? */
int unpack_hexdump(char src[], uint8_t dest[]) {
	int count = 0;
	while( src[count*2] > 32 ) {
		char c = fromhex(src[count*2]);
		char d = fromhex(src[count*2+1]);
		dest[count++] = (c<<4) + d;
	}
	return count;
}

int horus_loop( uint8_t *packet ) {
	int audiosize = sizeof( short ) * ( audio_input_iq ? 2 : 1 );
	short demod_in[max_demod_in * ( audio_input_iq ? 2 : 1 )];
	int max_ascii_out = horus_get_max_ascii_out_len( hstates ) | 1; // make sure it is an odd length
	char ascii_out[max_ascii_out];
	int len = 0;

	if ( fread( demod_in, audiosize, horus_nin( hstates ), stdin ) ==  horus_nin( hstates ) ) {
		int result;

		if ( audio_input_iq ) {
			result = horus_rx_comp( hstates, ascii_out, demod_in );
		} else {
			result = horus_rx( hstates, ascii_out, demod_in );
		}

		if ( result ) {
			ascii_out[max_ascii_out - 1] = 0; // make sure it`s a string
			len = unpack_hexdump(ascii_out, packet);
		}
	} else
		return -1;

	uint8_t i, f1, f2, f3, f4;
	struct MODEM_STATS stats;

	horus_get_modem_extended_stats( hstates, &stats );

	Config.freq = (int)stats.foff;
	Config.snr = (int)stats.snr_est;
        Config.ppm = (int)stats.clock_offset;
	f1 = (uint8_t)(stats.f_est[0] / 37.0) - 25; // convert 5 KHz to 160 range
	f2 = (uint8_t)(stats.f_est[1] / 37.0) - 25; // start at 1 kHz
	f3 = (uint8_t)(stats.f_est[2] / 37.0) - 25; // 1Kh - 6kHz in 133hz steps
	f4 = (uint8_t)(stats.f_est[3] / 37.0) - 25; // (160 >> 2) = 40 char display
	for (i=0; i < WATERFALL_SIZE; i++)
		Config.Waterfall[i] = 32; // " "

	Config.Waterfall[f1 >> 2] = 94; // "^"
	Config.Waterfall[f2 >> 2] = 94;
	Config.Waterfall[f3 >> 2] = 94;
	Config.Waterfall[f4 >> 2] = 94;

	Config.Waterfall[WATERFALL_SHOW] = 0;
	return len;
}
	/* TODO: need a horus_ function to dig into modem spectrum */
#if 0
	/* Print a sample of the FFT from the freq estimator */

	Ndft = hstates->fsk->Ndft / 2;
	for ( i = 0; i < Ndft; i++ ) {
		fprintf( stderr,"%f ",( hstates->fsk->fft_est )[i] );
		if ( i < Ndft - 1 ) {
			fprintf( stderr,"," );
		}
	}
#endif

void LogMessage( const char *format, ... ) {
	static WINDOW *Window = NULL;
	char Buffer[200];

	if ( Window == NULL ) {
		Window = newwin( 12, 99, 14, 0 );
		scrollok( Window, TRUE );
	}

	va_list args;
	va_start( args, format );
	vsnprintf( Buffer, 159, format, args );
	va_end( args );

	waddstr( Window, Buffer );
	wrefresh( Window );
}

void ChannelPrintf( int row, int column, const char *format, ... ) {
	char Buffer[80];

	va_list args;
	va_start( args, format );
	vsnprintf( Buffer, 40, format, args );
	va_end( args );

	mvwaddstr( Config.Window, row, column, Buffer );
}

void ChannelRefresh(void) {
	wrefresh( Config.Window );
}

void decode_callsign( char *callsign, uint8_t *codeptr ) {
	char *c, s;
	uint32_t code;

	*callsign = '\0';
	code = codeptr[0]  << 24;
	code |= codeptr[1] << 16;
	code |= codeptr[2] <<  8;
	code |= codeptr[3];

	/* Is callsign valid? */
	if ( code > 0xF423FFFF ) {
		code = 13 * 41; // "--"
	}

	for ( c = callsign; code; c++ )
	{
		s = code % 40;
		if ( s == 0 ) {
			*c = '-';
		} else if ( s < 11 ) {
			*c = '0' + s - 1;
		} else if ( s < 14 ) {
			*c = '-';
		} else { *c = 'A' + s - 14; }
		code /= 40;
	}
	*c = '\0';

	return;
}

void ConvertStringToHex( char *Target, char *Source, int Length ) {
	const char Hex[16] = "0123456789ABCDEF";
	int i;

	for ( i = 0; i < Length; i++ )
	{
		*Target++ = Hex[Source[i] >> 4];
		*Target++ = Hex[Source[i] & 0x0F];
	}

	*Target++ = '\0';
}

size_t write_data( void *buffer, size_t size, size_t nmemb, void *userp ) {
	return size * nmemb;
}

unsigned queued_images = 0;
char base64_ssdv[512 * 8];

void UploadMultiImages() {
	CURL *curl;
	char single[1000];  // 256 * base64 + headers
	char json[8000];  // 8 * single
	unsigned PacketIndex;
	char now[32];
	time_t rawtime;
	struct tm *tm;

	if ( !queued_images ) {
		return;
	}
	curl = curl_easy_init();
	if ( !curl ) {
		queued_images = 0;
		return;
	}

	time( &rawtime );
	tm = gmtime( &rawtime );
	strftime( now, sizeof( now ), "%Y-%0m-%0dT%H:%M:%SZ", tm );

	strcpy( json, "{\"type\": \"packets\",\"packets\":[" );
	for ( PacketIndex = 0; PacketIndex < queued_images; PacketIndex++ ) {
		snprintf( single, sizeof( single ),
				  "{\"type\": \"packet\", \"packet\": \"%s\", \"encoding\": \"base64\", \"received\": \"%s\", \"receiver\": \"%s\"}%s",
				  &base64_ssdv[PacketIndex * 512], now, Config.Tracker, ( queued_images - PacketIndex == 1 ) ? "" : "," );
		strcat( json, single );
	}
	strcat( json, "]}" );

	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_data );
	curl_easy_setopt( curl, CURLOPT_TIMEOUT, 20 );
	curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );
	curl_easy_setopt( curl, CURLOPT_HTTPHEADER, slist_headers );
	curl_easy_setopt( curl, CURLOPT_URL, "http://ssdv.habhub.org/api/v0/packets" );
	curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "POST" );
	curl_easy_setopt( curl, CURLOPT_COPYPOSTFIELDS, json );

	curlQueue( curl );
	queued_images = 0;
}

void UploadImagePacket( uint8_t *packet ) {
	size_t base64_length;

	base64_encode( packet, 256, &base64_length, &base64_ssdv[queued_images * 512] );
	base64_ssdv[base64_length + queued_images * 512] = '\0';
	if ( ++queued_images >= 8 ) {
		UploadMultiImages();
	}
}

void ReadString( FILE *fp, char *keyword, char *Result, int Length, int NeedValue ) {
	char line[100], *token, *value;

	fseek( fp, 0, SEEK_SET );
	*Result = '\0';

	while ( fgets( line, sizeof( line ), fp ) != NULL )
	{
		token = strtok( line, "= :\t" );
		if ( token && ( strcasecmp( keyword, token ) == 0 ) ) {
			value = strtok( NULL, ":= \t\n\r" );
			strcpy( Result, value );
			return;
		}
	}

	if ( NeedValue ) {
		LogMessage( "Missing value for '%s' in configuration file\n", keyword );
		exit( 1 );
	}
}

int ReadInteger( FILE *fp, char *keyword, int NeedValue, int DefaultValue ) {
	char Temp[64];

	ReadString( fp, keyword, Temp, sizeof( Temp ), NeedValue );

	if ( Temp[0] ) {
		return atoi( Temp );
	}

	return DefaultValue;
}

int ReadBoolean( FILE *fp, char *keyword, int NeedValue, int *Result ) {
	char Temp[32];

	ReadString( fp, keyword, Temp, sizeof( Temp ), NeedValue );

	if ( *Temp ) {
		*Result = ( *Temp == '1' ) || ( *Temp == 'Y' ) || ( *Temp == 'y' ) || ( *Temp == 't' ) || ( *Temp == 'T' );
	}

	return *Temp;
}

void LoadConfigFile() {
	FILE *fp;
	char *filename = "gateway.txt";
	char Keyword[32];

	Config.EnableHabitat = 1;
	Config.EnableSSDV = 1;
	Config.LogLevel = 0;
	Config.myLat = 52.0;
	Config.myLon = -2.0;
	Config.myAlt = 99.0;

	if ( ( fp = fopen( filename, "r" ) ) == NULL ) {
		printf( "\nFailed to open config file %s (error %d - %s).\nPlease check that it exists and has read permission.\n", filename, errno, strerror( errno ) );
		exit( 1 );
	}

	ReadString( fp, "tracker", Config.Tracker, sizeof( Config.Tracker ), 1 );
	LogMessage( "Tracker = '%s'\n", Config.Tracker );

	ReadBoolean( fp, "EnableHabitat", 0, &Config.EnableHabitat );
	ReadBoolean( fp, "EnableSSDV", 0, &Config.EnableSSDV );

	Config.LogLevel = ReadInteger( fp, "LogLevel", 0, 0 );

	sprintf( Keyword, "52" );
	ReadString( fp, "Latitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myLat );
	sprintf( Keyword, "-2" );
	ReadString( fp, "Longitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myLon );
	sprintf( Keyword, "99" );
	ReadString( fp, "Altitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myAlt );
	LogMessage( "Location: %lf, %lf, %lf\n", Config.myLat, Config.myLon, Config.myAlt );

	ReadBoolean( fp, "Mode", 1, &Config.Mode );
	LogMessage( "Mode = %s\n", Config.Mode ? "SSDV" : "Normal" );

	fclose( fp );
}

void LoadPayloadFile( int ID ) {
	FILE *fp;
	char filename[16];

	sprintf( filename, "payload_%d.txt", ID );

	if ( ( fp = fopen( filename, "r" ) ) != NULL ) {
		//LogMessage("Reading payload file %s\n", filename);
		ReadString( fp, "payload", Payloads[ID].Payload, sizeof( Payloads[ID].Payload ), 1 );
		LogMessage( "Payload %d = '%s'\n", ID, Payloads[ID].Payload );

		Payloads[ID].InUse = 1;

		fclose( fp );
	} else
	{
		strcpy( Payloads[ID].Payload, "Unknown" );
		Payloads[ID].InUse = 0;
	}
}

void LoadPayloadFiles( void ) {
	int ID;

	for ( ID = 0; ID < PAYLOAD_COUNT; ID++ )
	{
		LoadPayloadFile( ID );
	}
}

WINDOW * InitDisplay( void ) {
	WINDOW * mainwin;

	/*  Initialize ncurses  */
	if ( ( mainwin = initscr() ) == NULL ) {
		fprintf( stderr, "Error initialising ncurses.\n" );
		exit( EXIT_FAILURE );
	}

	start_color();
	init_pair( 1, COLOR_RED, COLOR_BLACK );
	init_pair( 2, COLOR_GREEN, COLOR_BLACK );

	color_set( 1, NULL );
	// bkgd(COLOR_PAIR(1));
	// attrset(COLOR_PAIR(1) | A_BOLD);

	// Title bar
	mvaddstr( 0, 10, " Horus Binary Habitat and SSDV Gateway " );
	refresh();

	Config.Window = newwin( 13, 34, 1, 0 );
	wbkgd( Config.Window, COLOR_PAIR( 2 ) );

	wrefresh( Config.Window );

	curs_set( 0 );

	return mainwin;
}

void CloseDisplay( WINDOW * mainwin ) {
	/*  Clean up after ourselves  */
	delwin( mainwin );
	endwin();
	refresh();
}

void DoPositionCalcs() {
	if ( Config.Latitude > 1e6 ) {
		Config.Latitude *= 1.0e-7;
		Config.Longitude *= 1.0e-7;
	}
	ChannelPrintf( 2, 1, "%8.5lf, %8.5lf, %05u   ",
				   Config.Latitude,
				   Config.Longitude,
				   Config.Altitude );

	/* See habitat-autotracker/autotracker/earthmaths.py. */
	double c = M_PI / 180;
	double lat1, lon1, lat2, lon2, alt1, alt2;
	lat1 = Config.myLat * c;
	lon1 = Config.myLon * c;
	alt1 = Config.myAlt;
	lat2 = Config.Latitude * c;
	lon2 = Config.Longitude * c;
	alt2 = Config.Altitude;

	double radius, d_lon, sa, sb, aa, ab, angle_at_centre, // bearing,
		   ta, tb, ea, eb, elevation, distance;

	radius = 6371000.0;

	d_lon = lon2 - lon1;
	sa = cos( lat2 ) * sin( d_lon );
	sb = ( cos( lat1 ) * sin( lat2 ) ) - ( sin( lat1 ) * cos( lat2 ) * cos( d_lon ) );
	// bearing = atan2(sa, sb) * (180/M_PI);
	aa = sqrt( ( sa * sa ) + ( sb * sb ) );
	ab = ( sin( lat1 ) * sin( lat2 ) ) + ( cos( lat1 ) * cos( lat2 ) * cos( d_lon ) );
	angle_at_centre = atan2( aa, ab );

	ta = radius + alt1;
	tb = radius + alt2;
	ea = ( cos( angle_at_centre ) * tb ) - ta;
	eb = sin( angle_at_centre ) * tb;

	elevation = atan2( ea, eb ) * ( 180 / M_PI );
	Config.Elevation = elevation;
	distance = sqrt( ( ta * ta ) + ( tb * tb ) -
					 2 * tb * ta * cos( angle_at_centre ) );
	Config.Distance = distance / 1000;

	ChannelPrintf( 1, 1, "%3.1lfkm, elevation %1.1lf  ", distance / 1000, elevation );
}


uint16_t CRC16( char *ptr, size_t len ) {
	uint16_t CRC, xPolynomial;
	int j;

	CRC = 0xffff;           // Seed
	xPolynomial = 0x1021;

	for (; len > 0; len-- )
	{   // For speed, repeat calculation instead of looping for each bit
		CRC ^= ( ( (unsigned int)*ptr++ ) << 8 );
		for ( j = 0; j < 8; j++ )
		{
			if ( CRC & 0x8000 ) {
				CRC = ( CRC << 1 ) ^ xPolynomial;
			} else {
				CRC <<= 1;
			}
		}
	}

	return CRC;
}


uint8_t Message[258];
int getPacket() {
	int result;
	result = horus_loop( &Message[1] );
	if (result < 0)
		return 0;
	Message[0] = (uint8_t)result;
	return 1;
}

int main( int argc, char **argv ) {
	uint8_t Bytes;
	uint32_t LoopCount;
	WINDOW * mainwin;

	curlInit();
	mainwin = InitDisplay();
	LogMessage( "**** Based on LoRa Gateway by daveake ****\n" );

	LoopCount = 0;
	Message[0] = 0;

	LoadConfigFile();
	LoadPayloadFiles();
	LogMessage( " * Press Control-C to quit *\n" );

	// TODO: check config for Mode
	if (!horus_init(Config.Mode))
		return -22;

	Config.LastPacketAt = time( NULL );
	while ( getPacket() && !curl_terminate )
	{
		Bytes = Message[0];
		Message[0] = 0;
		if ( Bytes > 0 ) {
			if ( Message[1] <= 32 ) {						/* Binary telemetry packet */
				struct TBinaryPacket BinaryPacket;
				char Data[100], Sentence[100];

				ChannelPrintf( 3, 1, "Binary Telemetry              " );
				memcpy( &BinaryPacket, &Message[1], sizeof( BinaryPacket ) );	

				decode_callsign( Config.Payload, (uint8_t *)&BinaryPacket.NameID );
				// TODO: legacy callsign
				// strcpy( Config.Payload, Payloads[0x1f & BinaryPacket.PayloadID] );

				Config.Seconds = BinaryPacket.Hours * 3600 +
								 BinaryPacket.Minutes * 60 +
								 BinaryPacket.Seconds;
				Config.Counter = BinaryPacket.Counter;
#if 0
				Config.Latitude = ( 1e-7 ) * BinaryPacket.Latitude.i;
				Config.Longitude = ( 1e-7 ) * BinaryPacket.Longitude.i;
#else
				Config.Latitude = (double)BinaryPacket.Latitude.f;
				Config.Longitude = (double)BinaryPacket.Longitude.f;
#endif
				Config.Altitude = BinaryPacket.Altitude;

				// if ( BinaryPacket.Checksum == CRC16( (char *)&Message[1], sizeof( BinaryPacket ) - 2 ) ) {
				{ // - Assume that checksum was confirmed by demod stage (?)
					sprintf( Data, "%s,%u,%02u:%02u:%02u,%1.5f,%1.5f,%u,%u,%u,%d,%1.2f",
							 Config.Payload,
							 BinaryPacket.Counter,
							 BinaryPacket.Hours,
							 BinaryPacket.Minutes,
							 BinaryPacket.Seconds,
							 Config.Latitude,
							 Config.Longitude,
							 Config.Altitude,
							 BinaryPacket.Speed,
							 BinaryPacket.Sats,
							 BinaryPacket.Temp,
							 5.0f / 255.0f * (float)BinaryPacket.BattVoltage );
					if (Message[1] == 32)	// Extended Packet
						sprintf(Data + strlen(Data), ",%d,%d", BinaryPacket.User1, BinaryPacket.User2);
					snprintf( Sentence, 100, "$$%s*%04X\n", Data, CRC16( Data, strlen( Data ) ) );

					UploadTelemetryPacket( Sentence );
					DoPositionCalcs();
					Config.TelemetryCount++;
#if 0
					sprintf( Sentence + strlen( Sentence ),
							 "\tStats:%1.1lf,%1.1lf,%d,%d\n",
							 Config.Distance,
							 Config.Elevation,
							 Config.rssi,
							 Config.snr );
#endif
					UpdatePayloadLOG( Sentence );
					LogMessage( "%s", Sentence );
				}
			} else if ( (Bytes == 255) && (Message[1] == 0x67) ) {					/* SSDV packet */
				char Callsign[8];

				decode_callsign( Callsign, &Message[2] );
				Callsign[7] = 0;

				// ImageNumber = Message[6];
				// PacketNumber = Message[8];

				LogMessage( "SSDV Packet, Callsign %s, Image %d, Packet %d\n",
							Callsign, Message[6], Message[7] * 256 + Message[8] );

				if ( Config.EnableSSDV ) {
					Message[0] = 0x55;             //  add SSDV sync byte at start of  packet
					UploadImagePacket( &Message[0] );
					Message[0] = 0x00;             //  also used to flag length of next packet
				}

				Config.SSDVCount++;
			} else
			{
				Config.UnknownCount++;
			}

			Config.LastPacketAt = time( NULL );
		}

		// redraw screen every second (horus loop rate)
		uint32_t interval;
		char *timescale = "s";

		interval = time( NULL ) - Config.LastPacketAt;
		if ( interval > 99 * 60 ) {
			interval /= 60 * 60;
			timescale = "h";
		} else if ( interval > 99 ) {
			interval /= 60;
			timescale = "m";
		}
		ChannelPrintf(  5, 1, "%u%s since last packet   ", interval, timescale );
		ChannelPrintf(  6, 1, "Telem Packets: %d   ", Config.TelemetryCount );
		ChannelPrintf(  7, 1, "Image Packets: %d   ", Config.SSDVCount );
		ChannelPrintf(  8, 1, "Bad CRC: %d Bad Type: %d", Config.BadCRCCount, Config.UnknownCount );
		ChannelPrintf(  9, 1, "Horus SNR: %4d   ", Config.snr );
		ChannelPrintf(  10, 1, "Horus PPM: %4d   ", Config.ppm );
		ChannelPrintf(  11, 1, "Horus Frequency: %4d   ", Config.freq );
		ChannelPrintf(  12, 1, "%s  ", Config.Waterfall );

		if ( ++LoopCount > 15 ) {     // no need for fast uploads
			LoopCount = 0;
			curlPush();
			UploadMultiImages();
			ChannelPrintf( 4, 1, "Uploads: %4d", curlUploads() );
		}
		ChannelRefresh();	// redraw ncurses display
		usleep( 300 * 1000 );   // short delay in case reading from file
	}

	CloseDisplay( mainwin );
	curlClean();
	horus_exit();
	return 0;
}
