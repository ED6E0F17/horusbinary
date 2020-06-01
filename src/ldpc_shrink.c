/*
  FILE...: ldpc_shrink.c
  CREATED: May 2020

  Downsample a wav file by 1/4, maintaining signal carier frequency
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define KISS_FFT_USE_ALLOCA
#include "_kiss_fft_guts.h"
/*forward complex FFT */
void kfc_fft( int nfft, const kiss_fft_cpx * fin,kiss_fft_cpx * fout );
/*reverse complex FFT */
void kfc_ifft( int nfft, const kiss_fft_cpx * fin,kiss_fft_cpx * fout );
/*free all cached objects*/
void kfc_cleanup( void );
#define PLEX kiss_fft_cpx
#define IPLEX( X ) X.r
#define QPLEX( X ) X.i
#define QSIZE       2048        /* input size = 48kHz / 47 Hz */
#define HSIZE   ( 2 * QSIZE )
#define KSIZE   ( 4 * QSIZE )   /* FFT size  = input * 4     */
#define HQSIZE  ( QSIZE / 2 )
#define QQSIZE  ( QSIZE / 4 )   /* Output size = input / 4   */
#define QQQSIZE ( QSIZE / 16 )  /* 64  entries for fading    */
#define UNDOCUMENTED_SCALING    KSIZE

/*
 * FFT filter and downsample 48kHz signal to 1/4 rate
 *	Downsample in the frequency domain to maintain offsets
 *
 *
 * 1024 block size in = 1/47 of a second, or half a symbol at 25Hz datarate
 *      4096 FFT => 12Hz bin size, grows to 48Hz bins at 4x output
 *		8K FFT =>  6Hz bins, 1/4 of the target bandwidth
 *	Buffer 4 blocks and overlap the fifth.
 */

int main( int argc, char *argv[] ) {
	unsigned i;
	int16_t dataio[2 * QSIZE];    // S16LE arrays for I/O
	PLEX td1[QSIZE];        // complex buffer for staging input
	PLEX td4[KSIZE];        // complex buffer for time domain
	PLEX fd4[KSIZE];        // complex buffer for frequency domain
	PLEX ds1[QSIZE];        // complex buffer for resampled output
	PLEX lapbuff[QQQSIZE];      // complex buffer for windowed audio out
	if ( 8 != sizeof( PLEX ) ) {
		fprintf( stderr, "Check: buffersize (2*4*1024) != %zu\n", sizeof( PLEX ) * QSIZE );
	}

	// generate raised cosine filter
	float filter[QSIZE];
	for ( i = 0; i < QSIZE; i++ )
		filter[i] = 0.5f + 0.5 * cosf( 3.1414927f * i / QSIZE );

	// The first block might be a wav header:
	if ( fread( dataio, 1, 64, stdin ) == 64 ) {
		fwrite( dataio, 1, 64, stdout );
	}


/* Loop until the pipe is empty */
	while ( fread( dataio, 2 * sizeof( int16_t ), QSIZE, stdin ) == QSIZE ) {
		unsigned j, m, n;

		// rotate fullbuffer scrubbing the first entry, refilling the end :
		memmove( td4, td4 + QSIZE, 3 * QSIZE * sizeof( PLEX ) );
		memcpy( td4 + 3 * QSIZE, td1, QSIZE * sizeof( PLEX ) );

		// reload fifth quarterbuffer
		for ( i = 0; i < QSIZE; i++ ) { // cast and normalise
			IPLEX( td1[i] ) = ( 1.0f / 32768 ) * (float)dataio[i * 2];
			QPLEX( td1[i] ) = ( 1.0f / 32768 ) * (float)dataio[i * 2 + 1];
		}

		// Window function to overlap first quarter buffer with fifth quarterbuffer
		for ( i = 0; i < QSIZE; i++ ) {
			float interpol = filter[i];
			IPLEX( td4[i] ) = interpol * IPLEX( td1[i] ) + ( 1.0f - interpol ) * IPLEX( td4[i] );
			QPLEX( td4[i] ) = interpol * QPLEX( td1[i] ) + ( 1.0f - interpol ) * QPLEX( td4[i] );
		}

		// Forward FFT to frequency domain
		kfc_fft( KSIZE, td4, fd4 );

		// Decimate frequency bins
		for ( i = 1,j = 1 + 3; i < QQSIZE / 2; i++,j += 3 ) { // Half QSIZE out is zeros, half mirrored, (*4 in KSIZE)
			m = KSIZE - j; // input from larger FFT
			n = QSIZE - i; // Downsample into smaller FFT
			IPLEX( fd4[i] ) = 0.5 * IPLEX( fd4[j] ) + IPLEX( fd4[j + 1] ) + IPLEX( fd4[j + 2] ) + 0.5 * IPLEX( fd4[j + 3] );
			QPLEX( fd4[i] ) = 0.5 * QPLEX( fd4[j] ) + QPLEX( fd4[j + 1] ) + QPLEX( fd4[j + 2] ) + 0.5 * QPLEX( fd4[j + 3] );
			IPLEX( fd4[n] ) = 0.5 * IPLEX( fd4[m] ) + IPLEX( fd4[m + 1] ) + IPLEX( fd4[m + 2] ) + 0.5 * IPLEX( fd4[m + 3] );
			QPLEX( fd4[n] ) = 0.5 * QPLEX( fd4[m] ) + QPLEX( fd4[m + 1] ) + QPLEX( fd4[m + 2] ) + 0.5 * QPLEX( fd4[m + 3] );
		}
		// filter out DC and  high frequencies
		IPLEX( fd4[0] ) = 0.0f;
		QPLEX( fd4[0] ) = 0.0f;
		for ( /* i */; i < n; i++ ) {
			IPLEX( fd4[i] ) = 0.0f;
			QPLEX( fd4[i] ) = 0.0f;
		}

		// Invert 1/4 FFT to time domain
		kfc_ifft( QSIZE, fd4, ds1 );

		// Window Audio out: Keep 25% of previous block for cross fading.

		// Extract midsection of samples
		j = QQSIZE * 2; // 3rd of 4 quarters
		for ( i = 0; i < QQQSIZE; i++ ) {
			dataio[i * 2] = (int16_t)( ( IPLEX( ds1[i + j] ) * 2.0f * i / QQQSIZE )
					+ ( IPLEX( lapbuff[i] ) * 2.0f * ( 1.0 - (float)i / QQQSIZE ) ) );
			dataio[i * 2 + 1] = (int16_t)( ( QPLEX( ds1[i + j] ) * 2.0f * 1 / QQQSIZE )
					+ ( QPLEX( lapbuff[i] ) * 2.0f * ( 1.0 - (float)i / QQQSIZE ) ) );
		}

		for ( /* i */; i < QQSIZE; i++ ) {
			dataio[i * 2] = (int16_t)( IPLEX( ds1[i + j] ) * 2.0f );
			dataio[i * 2 + 1] = (int16_t)( QPLEX( ds1[i + j] ) * 2.0f );
		}

		for ( /* i */; i < QQSIZE + QQQSIZE; i++ ) {
			IPLEX( lapbuff[i - QQSIZE] ) =  IPLEX( ds1[i + j] );
			QPLEX( lapbuff[i - QQSIZE] ) =  QPLEX( ds1[i + j] );
		}

		fwrite( dataio, 2 * sizeof( int16_t ), QQSIZE, stdout );
	}

	kfc_cleanup();
	return 0;
}
