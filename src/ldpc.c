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
#include "mpdecode.h"
#include "H2064_516_sparse.h"

#define BYTES_PER_PACKET       256
#define CRC_BYTES              2
#define PARITY_BYTES           65
#define BITS_PER_BYTE          8
#define SYMBOLS_PER_PACKET     ((BYTES_PER_PACKET + CRC_BYTES + PARITY_BYTES) * BITS_PER_BYTE)
#define HORUS_SSDV_NUM_BITS    2616    /* (32 + (258 + 65) * 8) */

/* Scramble and interleave are 8bit lsb, but bitstream is sent msb */
#define LSB2MSB(X) (X + 7 - 2 * (X & 7) )

/* Invert bits - ldpc expects negative floats for high hits */
void unscramble(float *in, float* out) {
	int i, ibit;
	uint16_t scrambler = 0x4a80;  /* init additive scrambler at start of every frame */
	uint16_t scrambler_out;

	for ( i = 0; i < SYMBOLS_PER_PACKET; i++ ) {
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

void deinterleave(float *in, float* out) {
	int b, n, i, j;

	b = 337; /* Largest Prime number less than nbits for a 22 byte packet */
	for ( n = 0; n < SYMBOLS_PER_PACKET; n++ ) {
		i = LSB2MSB(n);
		/* FFS put brackets around macros: a%b*c != a%(b*c) */
		j = LSB2MSB( (b * n) % SYMBOLS_PER_PACKET);
		out[i] = in[j];
	}
}


/* LDPC decode */
void horus_ldpc_decode(uint8_t *payload, float *sd) {
	float sum, mean, sumsq, estEsN0, x;
	float llr[HORUS_SSDV_NUM_BITS];
	float temp[HORUS_SSDV_NUM_BITS];
	uint8_t outbits[HORUS_SSDV_NUM_BITS];
	int b, i, parityCC;
	struct LDPC ldpc;

	/* normalise bitstream to log-like */
	sum = 0.0;
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ )
		sum += fabs(sd[i]);
	mean = sum / HORUS_SSDV_NUM_BITS;

	sumsq = 0.0;
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ ) {
		x = fabs(sd[i]) / mean - 1.0;
		sumsq += x * x;
	}
	estEsN0 =  2.0 * HORUS_SSDV_NUM_BITS / (sumsq + 1.0e-3);
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ )
		llr[i] = estEsN0 * sd[i];

	/* reverse whitening and re-order bits */
	unscramble(llr, temp);
	deinterleave(temp, llr);

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

	i = run_ldpc_decoder(&ldpc, outbits, llr, &parityCC);

	/* convert MSB bits to a packet of bytes */    
	for (b=0; b<BYTES_PER_PACKET; b++) {
		uint8_t rxbyte = 0;
		for(i=0; i<8; i++)
			rxbyte |= outbits[b*8+i] << (7 - i);
		payload[b] = rxbyte;
	}
}
