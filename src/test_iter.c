/* ldpc interface to decoder
 *
 * 	It is expected that the switch to ldpc will give a 60% speed improvement
 * over golay code, with no loss of performance over white noise - the use of
 * soft-bit detection and longer codewords compensating for the expected 2dB loss
 * from reducing the number of parity bits.
 *
 * Golay code can reliably correct a 10% BER, equivalent to a 20% loss of signal
 * during deep fading. It is not clear how well ldpc will cope with deep fading,
 * but the shorter packers are bound to be more badly affected.
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "math.h"
#include "string.h"
#include "mpdecode.h"
#include "horus_l2.h"
#include "HRA128_384.h"

// Need a sensible prime number for interleaving, but using the same value
// as Horus binary 22 byte golay code also works.... Check it is coprime!

#define BITS_PER_PACKET     ((DATA_BYTES + PARITY_BYTES) * 8)

/* Scramble and interleave are 8bit lsb, but bitstream is sent msb */
#define LSB2MSB(X) (X + 7 - 2 * (X & 7) )

/* Compare detected bits to corrected bits */
int count_errors( uint8_t *outbytes, uint8_t *inbytes ) {
	int	i, count = 0;

	// count bits changed during error correction
	for(i = 0; i < BITS_PER_PACKET; i++) {
		int x, y, offset, shift;

		shift = i & 7;
		offset = i >> 3;
		x = inbytes[offset] >> shift;
		y = outbytes[offset] >> shift;
		count += (x ^ y) & 1;
	}

	return count;
}


/*
 * Command line options:
 *	1.0 "noise_scale" => bad bits are  half the size of good bits
 *		implying a noise peak of 1.5 * signal
 *		( good bit noise peak is 0.5 * signal ) 
 *
 *	1.0 "log_scale" is Default log-like scaling
 *		increasing scale is good if errors are small
 *		- but how would you know ?
 */
float noise_scale = 0.5f; // moderate noise level
float log_scale = 1.0f;	// use 3.2f  to trigger issue
int set_errors = 40;	// (max) number of errors to set

/* normalise bitstream to log-like */
void loglike(float *sd, float *llr) {
	float sum, mean, sumsq, estEsN0, x;
	int i;

	sum = 0.0;
	for ( i = 0; i < BITS_PER_PACKET; i++ )
		sum += fabs(sd[i]);
	mean = sum / BITS_PER_PACKET;

	sumsq = 0.0;
	for ( i = 0; i < BITS_PER_PACKET; i++ ) {
		x = fabs(sd[i]) / mean - 1.0;
		sumsq += x * x;
	}
	estEsN0 =  log_scale * 1.0 * BITS_PER_PACKET / (sumsq + 1.0e-3) / mean;
	for ( i = 0; i < BITS_PER_PACKET; i++ )
		llr[i] = estEsN0 * sd[i];
}

/* LDPC decode */
void test_ldpc_decode(uint8_t *payload, float *sd) {
	float llr[BITS_PER_PACKET];
	uint8_t outbits[BITS_PER_PACKET];
	int b, i, parityCC;
	struct LDPC ldpc;

	/* correct errors */
	loglike(sd, llr);

	ldpc.max_iter = MAX_ITER;
	ldpc.dec_type = 0;
	ldpc.q_scale_factor = 1;
	ldpc.r_scale_factor = 1;
	ldpc.CodeLength = CODELENGTH;
	ldpc.NumberParityBits = NUMBERPARITYBITS;
	ldpc.NumberRowsHcols = NUMBERROWSHCOLS;
	ldpc.max_row_weight = MAX_ROW_WEIGHT;
	ldpc.max_col_weight = MAX_COL_WEIGHT;
	ldpc.H_rows = H_rows;
	ldpc.H_cols = H_cols;

	i = run_ldpc_decoder(&ldpc, outbits, llr, &parityCC);

	/* convert MSB bits to a packet of bytes */    
	for (b = 0; b < DATA_BYTES + PARITY_BYTES; b++) {
		uint8_t rxbyte = 0;
		for(i=0; i<8; i++)
			rxbyte |= outbits[b*8+i] << (7 - i);
		payload[b] = rxbyte;
	}
}

#define MYBYTES (DATA_BYTES + PARITY_BYTES)
#define MYBITS (MYBYTES * 8)

int test(uint8_t *input, int punctures) {
	float	fin[MYBITS];
	uint8_t output[MYBYTES];
	int	i;

	// Convert to (inverted )floats with -3dB noise
	for (i=0; i< MYBITS; i++) {
		int inbit = input[i>>3] >> (7-(i&7));
		float noise = -2.1f + sinf((float)i * 0.3f); // scale +/- 50%
		fin[i] = noise * ( 2.0f * (float)(inbit&1) - 1.0);
	}

	// Puncture
	for (i=0; i < punctures; i++) {
		int index = (17 * i + 5) % MYBITS;
		// flip bits by changing sign, bad bits are smaller than good bits
		fin[index] *= -0.5f * noise_scale;
	}

	// Decode
	test_ldpc_decode(output, fin);

#if 0
	// Log result
	for (i=0; i<MYBYTES; i++) {
		uint8_t hex=output[i] ^ input[i];
		fprintf(stderr, "%02x", hex);
	}
#endif
	i = count_errors(input, output);

	fprintf(stderr, ": %2d%% BER: %s\n", 100*punctures/MYBITS, i?"Bad":"Good");
	return i;
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

int main(int argc, char **argv) {
	uint8_t *pout;
	uint8_t input[MYBYTES];
	int opt, i, last = 0;

	while ((opt = getopt(argc, argv, "l:n:e:h")) != -1) {
		switch (opt) {
		case 'n':
			noise_scale = atof(optarg);
			break;
		case 'l':
			log_scale = atof(optarg);
			break;
		case 'e':
			set_errors = (int)atof(optarg);
			break;
		case 'h':
		default:
			printf("Usage: test -l (loglike) -n (noise) -e (errors)\n");
			printf("\te.g.\t./test -e 40 -l 1.5 -n 0.2\n");
			exit(0);
		}
	}
	
	verbose_ldpc(2); // set loglevel for decoder

	// generate pseudorandom bits
	memset(input, 0, MYBYTES);
	scramble(input, DATA_BYTES);
	pout = (input + DATA_BYTES);

	// process parity bit offsets
	for (i = 0; i < NUMBERPARITYBITS; i++) {
		unsigned int shift, j;

		for(j = 0; j < MAX_ROW_WEIGHT; j++) {
			uint16_t tmp  = H_rows[ j * NUMBERPARITYBITS  + i ] - 1;

			shift = 7 - (tmp & 7); // MSB
			last ^= input[tmp >> 3] >> shift;
		}
		shift = 7 - (i & 7); // MSB
		pout[i >> 3] |= (last & 1) << shift;
	}

	// test with "i" added  errors
	for (i=1; i <= set_errors; i++) {
		fprintf(stderr, "%02d,", i);
		if (test (input, i))
			break;
	}

	return 0;
}

