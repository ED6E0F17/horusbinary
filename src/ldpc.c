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


#include <stdint.h>
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

/* Invert bits - ldpc expects negative floats for high hits */
void unscramble(float *in, float* out) {
	int i, ibit;
	uint16_t scrambler = 0x4a80;  /* init additive scrambler at start of every frame */
	uint16_t scrambler_out;

	for ( i = 0; i < BITS_PER_PACKET; i++ ) {
		scrambler_out = ( (scrambler >> 1) ^ scrambler) & 0x1;

		/* modify i-th bit by xor-ing with scrambler output sequence */
		ibit = LSB2MSB(i);
		if ( scrambler_out ) {
			out[ibit] = in[ibit];
		} else {
			out[ibit] = -in[ibit];
		}

		scrambler >>= 1;
		scrambler |= scrambler_out << 14;
	}
}

// soft bit deinterleave
void deinterleave(float *in, float* out) {
	int n, i, j;

	for ( n = 0; n < BITS_PER_PACKET; n++ ) {
		i = LSB2MSB(n);
		j = LSB2MSB( (COPRIME * n) % BITS_PER_PACKET);
		out[i] = in[j];
	}
}

// packed bit deinterleave - same as Golay version , but different Coprime
void bitwise_deinterleave(uint8_t *inout, int nbytes)
{
    uint16_t nbits = (uint16_t)nbytes*8;
    uint32_t i, j, ibit, ibyte, ishift, jbyte, jshift;
    uint8_t out[nbytes];

    memset(out, 0, nbytes);
    for(j = 0; j < nbits; j++) {
        i = (COPRIME * j) % nbits;

        /* read bit i */
        ibyte = i>>3;
        ishift = i&7;
        ibit = (inout[ibyte] >> ishift) & 0x1;

	/* write bit i to bit j position */
        jbyte = j>>3;
        jshift = j&7;
        out[jbyte] |= ibit << jshift;
    }
 
    memcpy(inout, out, nbytes);
}

/* Compare detected bits to corrected bits */
void ldpc_errors( const uint8_t *outbytes, uint8_t *rx_bytes ) {
	int	length = DATA_BYTES + PARITY_BYTES;
	uint8_t temp[length];
	int	i, percentage, count = 0;
	memcpy(temp, rx_bytes, length);

	scramble(temp, length); // use scrambler from Golay code
	bitwise_deinterleave(temp, length);

	// count bits changed during error correction
	for(i = 0; i < BITS_PER_PACKET; i++) {
		int x, y, offset, shift;

		shift = i & 7;
		offset = i >> 3;
		x = temp[offset] >> shift;
		y = outbytes[offset] >> shift;
		count += (x ^ y) & 1;
	}

	// scale errors against a maximum of 20% BER
	percentage = (count * 5 * 100) / BITS_PER_PACKET;
	if (percentage > 100)
		percentage = 100;
	set_error_count( percentage );
}

#define PREDICTBYTES 14
static uint8_t history[PREDICTBYTES * 8];
static uint8_t future[PREDICTBYTES * 8];
static int use_history = 0;
int known[PREDICTBYTES] = {8, 7, 8,4, 8,8,3, 8,8,3, 8,3, 8, 8}; // Expected unchanged bits

void confirm_good(int ok) {
	int i;
	if (ok) {
		use_history = 5;
		for ( i = 0; i < PREDICTBYTES*8; i++ )
			history[i] = future[i];
	} else {
		if (use_history)
			use_history--;
	}
}

void predict(float *softbits) {
	int i, j;
	float data;
	float weight; // predicted data

	for (i=0; i < PREDICTBYTES ; i++) {
		for (j = 0; j < known[i]; j++) {
			data = 1.0f - 2.0f * history[i*8+j]; // bits are active low
			weight = (known[i] + 12 - j) * 0.3f;
			if ( softbits[i*8+j] * data > 0 )
				softbits[i*8+j] *= weight;
			else
				softbits[i*8+j] /= weight * 2;
		}
	}
}


/* LDPC decode */
void horus_ldpc_decode(uint8_t *payload, float *sd) {
	float sum, mean, sumsq, estEsN0, x;
	float llr[BITS_PER_PACKET];
	float temp[BITS_PER_PACKET];
	uint8_t outbits[BITS_PER_PACKET];
	int b, i, parityCC;
	struct LDPC ldpc;

	/* normalise bitstream to log-like */
	sum = 0.0;
	for ( i = 0; i < BITS_PER_PACKET; i++ )
		sum += fabs(sd[i]);
	mean = sum / BITS_PER_PACKET;

	sumsq = 0.0;
	for ( i = 0; i < BITS_PER_PACKET; i++ ) {
		x = fabs(sd[i]) / mean - 1.0;
		sumsq += x * x;
	}
	estEsN0 = 1.0f * BITS_PER_PACKET / (sumsq + 1.0e-3) / mean;
	for ( i = 0; i < BITS_PER_PACKET; i++ )
		llr[i] = estEsN0 * sd[i];

	/* reverse whitening and re-order bits */
	unscramble(llr, temp);

	// deinterleave(temp, llr);			/* Interleave disabled */

	/* correct errors */
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

	if (use_history)
		predict(llr);
	i = run_ldpc_decoder(&ldpc, outbits, llr, &parityCC);
	for ( i = 0; i < PREDICTBYTES*8; i++ )
		future[i] = outbits[i];

	/* convert MSB bits to a packet of bytes */    
	for (b = 0; b < DATA_BYTES + PARITY_BYTES; b++) {
		uint8_t rxbyte = 0;
		for(i=0; i<8; i++)
			rxbyte |= outbits[b*8+i] << (7 - i);
		payload[b] = rxbyte;
	}
}
