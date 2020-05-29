/*
  FILE...: ldpc_enc.c
  AUTHOR.: Don Reid
  CREATED: Aug 2018

  Add noise to LDPC soft decision samples for testing.  Simulates use
  of LDPC code with 4FSK modem.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>

int main( int argc, char *argv[] ) {
	FILE        *fin, *fout;
	double datain, dataout;

	if ( argc < 3 ) {
		fprintf( stderr, "\n" );
		fprintf( stderr, "usage: %s InputFile OutputFile NodB\n", argv[0] );
		fprintf( stderr, "\n" );
		exit( 1 );
	}

	if ( strcmp( argv[1], "-" )  == 0 ) {
		fin = stdin;
	} else if ( ( fin = fopen( argv[1],"rb" ) ) == NULL )  {
		fprintf( stderr, "Error opening input bit file: %s: %s.\n",
				 argv[1], strerror( errno ) );
		exit( 1 );
	}

	if ( strcmp( argv[2], "-" ) == 0 ) {
		fout = stdout;
	} else if ( ( fout = fopen( argv[2],"wb" ) ) == NULL )  {
		fprintf( stderr, "Error opening output bit file: %s: %s.\n",
				 argv[2], strerror( errno ) );
		exit( 1 );
	}

	double NodB = atof( argv[3] );
	double No = pow( 10.0, NodB / 10.0 );
	double sum_xx = 0; double sum_x = 0.0; long n = 0;

	// for 4fsk we have two bits, but only pick one here
	// No is constant, but  Eb is halved ?
	// => Getting sensible results approaching the Shannon Limit
	fprintf( stderr, "Coded 4FSK Eb/No simulation.\n" );
	fprintf( stderr, "Noise simulates 4FSK, Adjusting EbNo for 1/3 coderate:\n" );
	fprintf( stderr, "Noise = % 4.2f dB, Eb/No = %4.2f dB\n", NodB,  2.8 - NodB );

	while ( fread( &datain, sizeof( double ), 1, fin ) == 1 ) {
		int i;
		double x,y,z,noise[4],noisediff;
		// for 4fsk we need 4 bits of noise, two for the signal bit, and two for not
		//  and then ? add the (largest sig+,  less the largest not+)
		for ( i = 0; i < 4; i++ ) {
			// Gaussian from uniform:
			x = (double)rand() / RAND_MAX;
			y = (double)rand() / RAND_MAX;
			z = sqrt( -2 * log( x ) ) * cos( 2 * M_PI * y );

			noise[i] = sqrt( No / 2 ) * z;
		}
		// assume data is positive for now
		if ( noise[1] > noise [0] ) {
			noise[0] = noise[1];
		}
		if ( noise[3] > noise [2] ) {
			noise[2] = noise[3];
		}
		noisediff = noise[0] - noise[2];

		// now adjust sign
		if ( datain > 0 ) {
			dataout = datain + noisediff;
		} else {
			dataout = datain - noisediff;
		}

		fwrite( &dataout, sizeof( double ), 1, fout );

		// keep running stats to calculate actual noise variance (power)
		sum_xx += noisediff * noisediff;
		sum_x  += noisediff;
		n++;
	}

	fclose( fin );
	fclose( fout );

	double noise_var = ( n * sum_xx - sum_x * sum_x ) / ( n * ( n - 1 ) );
	fprintf( stderr, "measured double sided (real) noise power: %f\n", noise_var );

	return 0;
}
