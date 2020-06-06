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
struct horus *hstates;
int horus_mode = 0;
int audioIQ = 0;
int max_demod_in = 0;

/* It would be possible to run a single input through the decoder at two speeds */
int horus_init( int mode ) {
	if (mode == 1)
		horus_mode = HORUS_MODE_LDPC;
	else if (mode == 2)
		horus_mode = HORUS_MODE_RTTY;
	else if (mode == 3)
		horus_mode = HORUS_MODE_PITS;
	else
		horus_mode = HORUS_MODE_BINARY;

	hstates = horus_open( horus_mode );
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
	int audiosize = sizeof( short ) * ( audioIQ ? 2 : 1 );
	short demod_in[max_demod_in * ( audioIQ ? 2 : 1 )];
	int max_ascii_out = horus_get_max_ascii_out_len( hstates ) | 1; // make sure it is an odd length
	char ascii_out[max_ascii_out];
	int len = 0;

	if ( fread( demod_in, audiosize, horus_nin( hstates ), stdin ) ==  horus_nin( hstates ) ) {
		int result;

		if ( audioIQ ) {
			result = horus_rx_comp( hstates, ascii_out, demod_in );
		} else {
			result = horus_rx( hstates, ascii_out, demod_in );
		}

		if ( result ) {
			ascii_out[max_ascii_out - 1] = 0; // make sure it`s a string
			if((horus_mode == HORUS_MODE_RTTY) || (horus_mode == HORUS_MODE_PITS))
				len = sprintf((char *)packet, "%s\n", ascii_out);
			else
				len = unpack_hexdump(ascii_out, packet);
		}
	} else
		return -1;

	uint8_t i, f1, f2, f3, f4;
	struct MODEM_STATS stats;

	horus_get_modem_extended_stats( hstates, &stats );

	Config.freq = (int)(-0.1 * stats.foff) * 10;
	Config.snr = (int)stats.snr_est - 10; // random scaling
        Config.ppm = (int)stats.clock_offset;

	for (i=0; i < WATERFALL_SIZE; i++)
		Config.Waterfall[i] = 32; // " "
	f1 = (uint8_t)(stats.f_est[0] / 37.0) - 25; // convert 5 KHz to 160 range
	f2 = (uint8_t)(stats.f_est[1] / 37.0) - 25; // start at 1 kHz
	Config.Waterfall[f1 >> 2] = 94; // "^"
	Config.Waterfall[f2 >> 2] = 94;
	Config.Waterfall[WATERFALL_SHOW] = 0;
	if((horus_mode == HORUS_MODE_RTTY) || (horus_mode == HORUS_MODE_PITS))
		return len;	// 2 fsk

	f3 = (uint8_t)(stats.f_est[2] / 37.0) - 25; // 1Kh - 6kHz in 133hz steps
	f4 = (uint8_t)(stats.f_est[3] / 37.0) - 25; // (160 >> 2) = 40 char display
	Config.Waterfall[f3 >> 2] = 94;
	Config.Waterfall[f4 >> 2] = 94;
	Config.Waterfall[WATERFALL_SHOW] = 0;
	return len;		// 4 fsk
}

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
	char Payload[PAYLOAD_SIZE];
	int i;

	Config.EnableHabitat = 1;
	Config.EnableSSDV = 1;
	Config.myLat = 52.0;
	Config.myLon = -2.0;
	Config.myAlt = 99.0;

	if ( ( fp = fopen( filename, "r" ) ) == NULL ) {
		printf( "\nFailed to open config file %s (error %d - %s).\nPlease check that it exists and has read permission.\n", filename, errno, strerror( errno ) );
		exit( 1 );
	}

	ReadString( fp, "Tracker", Config.Tracker, sizeof( Config.Tracker ), 1 );
	ReadBoolean( fp, "EnableHabitat", 0, &Config.EnableHabitat );
	ReadBoolean( fp, "EnableSSDV", 0, &Config.EnableSSDV );

	sprintf( Keyword, "52" );
	ReadString( fp, "Latitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myLat );
	sprintf( Keyword, "-2" );
	ReadString( fp, "Longitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myLon );
	sprintf( Keyword, "99" );
	ReadString( fp, "Altitude", Keyword, sizeof( Keyword ), 0 );
	sscanf( Keyword, "%lf", &Config.myAlt );
	Config.Mode = ReadInteger( fp, "Mode", 0, 0 );
	for (i = 0; i < PAYLOAD_COUNT; i++) {
		sprintf( Config.Payloads[i], "ID%d", i );
		ReadString( fp, Config.Payloads[i], Payload, PAYLOAD_SIZE, 0);
		if(Payload[0]) {
			snprintf(Config.Payloads[i], PAYLOAD_SIZE, "%s", Payload);
		}
	}
	fclose( fp );
}

void LogConfigFile(void) {
	int i;
	LogMessage( "Tracker = '%s'\n", Config.Tracker );
	LogMessage( "Location: %lf, %lf, %lf\n", Config.myLat, Config.myLon, Config.myAlt );

	char *modestring = "Horus Binary";
	if (Config.Mode == 1)
		modestring = "Horus 25Hz";
	else if (Config.Mode == 2)
		modestring = "RTTY100 7N2";
	else if (Config.Mode == 3)
		modestring = "RTTY300 8N2";
	LogMessage( "Mode = %s\n", modestring );

	LogMessage("Payloads List:");
	for (i = 0; i < PAYLOAD_COUNT; i++)
		LogMessage("%s,",Config.Payloads[i]);
	LogMessage("from gateway.txt\n");
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
	mvaddstr( 0, 3, " Horus Binary and RTTY Habitat Gateway " );
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

	ChannelPrintf( 1, 1, "%3.1lfkm, elevation %1.1lf      ", distance / 1000, elevation );
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
	if (result < 0) // EOF
		return 0;
	Message[0] = (uint8_t)result;
	return 1;
}

int main( int argc, char **argv ) {
	uint8_t Bytes;
	WINDOW * mainwin;

	int o = getopt(argc,argv,"hq");
	switch(o) {
	case -1:
		break;
	case 'q':
		audioIQ = 1;
		break;
	default:
	case 'h':
		fprintf(stderr, "Horus Binary Gateway based on LoRa version.\n");
		fprintf(stderr, "\tUsage: \"cat S16LE_48K.wav | gateway\"\n");
		fprintf(stderr, "\t       \"  (resample audio to 48kHz)\"\n");
		fprintf(stderr, "\tOption: [-q] uses stereo (iq) input.\n");
		fprintf(stderr, "\tConfig: Edit \"gateway.txt\" file.\n\n");
		exit(0);
	}

	Message[0] = 0;
	LoadConfigFile();

	if (!horus_init(Config.Mode))
		return -22;

	fprintf(stderr, "Usage: \"cat S16LE_48K.wav | gateway\" (use audio at 48kHz)\n");
	fprintf(stderr, "Press Control-C to Quit, if there is no input file.\n");
	if (!getPacket())
		exit(0);  // supplied file shorter than 1s ?

	curlInit();
	mainwin = InitDisplay();
	LogConfigFile(); // Cannot display results before this

	Config.LastPacketAt = time( NULL );
	while ( getPacket() && !curl_terminated() )
	{
		Bytes = Message[0];
		Message[0] = 0;
		if ( Bytes ) {
			if ((horus_mode == HORUS_MODE_RTTY) || (horus_mode == HORUS_MODE_PITS)) {	 /* UKHAS ASCII String */
				char *Sentence = (char *)&Message[1];
				Sentence[Bytes] = 0;
				UpdatePayloadLOG( Sentence );
				LogMessage( "%s", Sentence );
				UploadTelemetryPacket( Sentence );
				// DoPositionCalcs();
				ChannelPrintf( 3, 1, " RTTY  Telemetry                " );
				Config.RTTYCount++;
			} else if (Bytes == 16) {							/* Short 14 byte packet */
				struct SBinaryPacket BinaryPacket;
				char Data[90], Sentence[100];
				int position;
			        unsigned hours, minutes, seconds;
				int16_t user, temp, sats;
				float volts;

				ChannelPrintf( 3, 1, " LDPC  Telemetry              " );
				memcpy( &BinaryPacket, &Message[1], sizeof( BinaryPacket ) );
				gray2bin( (uint8_t*)&BinaryPacket, sizeof BinaryPacket);

				strcpy( Config.Payload, Config.Payloads[0x1f & BinaryPacket.PayloadID] );
				Config.Counter = BinaryPacket.Counter;
				Config.Seconds = BinaryPacket.BiSeconds * 2;
				hours =  (Config.Seconds / 3600);
				minutes =  (Config.Seconds / 60) - (hours * 60);
				seconds =  Config.Seconds - (hours * 3600) - (minutes * 60);

				position = ((int)(int8_t)BinaryPacket.Latitude[2] << 24) |
						((uint8_t)BinaryPacket.Latitude[1] <<16) |
						((uint8_t)BinaryPacket.Latitude[0] << 8);
				Config.Latitude = (double)position * 1.0e-7;
				position = ((int)(int8_t)BinaryPacket.Longitude[2] << 24) |
						((uint8_t)BinaryPacket.Longitude[1] <<16) |
						((uint8_t)BinaryPacket.Longitude[0] << 8);
				Config.Longitude = (double)position * 1.0e-7 ;
				Config.Altitude = BinaryPacket.Altitude;
				user = BinaryPacket.User;
				sats = (user & 0x3) << 2;	// 0,4,8,12
				temp = (int8_t)user >> 2;	//-32 to 31
				volts = 5.0f / 255.0f * (float)BinaryPacket.Voltage;

				{ // - Assume that checksum was confirmed by demod stage (?)
					snprintf( Data, 90, "%s,%d,%02u:%02u:%02u,%1.5f,%1.5f,%u,0,%d,%d,%0.2f",
							 Config.Payload, Config.Counter,
							 hours, minutes, seconds,
							 Config.Latitude, Config.Longitude,
							 Config.Altitude, //speed
							 sats, temp, volts);
					snprintf( Sentence, 100, "$$%s*%04X\n", Data, CRC16( Data, strlen( Data ) ) );
				}

				LogMessage( "%s", Sentence );
				UploadTelemetryPacket( Sentence );
				UpdatePayloadLOG( Sentence );
				DoPositionCalcs();
				Config.LDPCCount++;

			} else if (Bytes == 22) {							/* Horus Binary 22 byte packet */
				struct TBinaryPacket BinaryPacket;
				char Data[90], Sentence[100];

				ChannelPrintf( 3, 1, "Binary Telemetry              " );
				memcpy( &BinaryPacket, &Message[1], sizeof( BinaryPacket ) );

				strcpy( Config.Payload, Config.Payloads[0x1f & BinaryPacket.PayloadID] );

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
					snprintf( Sentence, 100, "$$%s*%04X\n", Data, CRC16( Data, strlen( Data ) ) );

					UploadTelemetryPacket( Sentence );
					DoPositionCalcs();
					Config.BinaryCount++;
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
		ChannelPrintf(  4, 1, "%u%s since last packet   ", interval, timescale );
		ChannelPrintf(  5, 1, " RTTY  Rx: %3d   ", Config.RTTYCount );
		ChannelPrintf(  6, 1, "Binary Rx: %3d   ", Config.BinaryCount );
		ChannelPrintf(  7, 1, " LDPC  Rx: %3d   ", Config.LDPCCount );
		ChannelPrintf(  8, 1, "Bad CRC: %3d, Quality: %d  ", horus_bad_crc(), horus_quality() );
		ChannelPrintf(  9, 1, "Est.SNR: %3d, PPM: %d    ", Config.snr, Config.ppm );
		ChannelPrintf(  10, 1, "Uploads: %3d     ", curlUploads() );
		ChannelPrintf(  11, 1, "Frequency: %3d   ", Config.freq );
		ChannelPrintf(  12, 1, "%s  ", Config.Waterfall );
		ChannelRefresh();	// redraw ncurses display
		curlPush();		// Upload now
		usleep( 200 * 1000 );	// short delay in case reading from file
	}

	LogMessage("Shutting down.\n");
	curlPush();		// Upload now
	usleep( 1500 * 1000 );	// very short delay for uploads
	CloseDisplay( mainwin );
	curlClean();
	horus_exit();
	return 0;
}
