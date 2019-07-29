#include <stdint.h>
#include "math.h"
#include "mpdecode_core.h"
#include "H2064_516_sparse.h"

#define BYTES_PER_PACKET       256
#define CRC_BYTES              2
#define PARITY_BYTES           65
#define BITS_PER_BYTE          8
#define UNPACKED_PACKET_BYTES  ( (UW_BYTES + BYTES_PER_PACKET + CRC_BYTES) * BITS_PER_BYTE)
#define SYMBOLS_PER_PACKET     (BYTES_PER_PACKET + CRC_BYTES + PARITY_BYTES) * BITS_PER_BYTE
#define HORUS_SSDV_NUM_BITS    2616    /* image data (32 + (258 + 65) * 8) */

/* Scramble and interleave are 8bit lsb, but bitstream is sent msb */
#define LSB2MSB(X) (X + 7 - 2 * (X & 7) )

void unscramble(float *in, float* out) {
	int i, ibit;
	uint16_t scrambler = 0x4a80;  /* init additive scrambler at start of every frame */
	uint16_t scrambler_out;

	for ( i = 0; i < SYMBOLS_PER_PACKET; i++ ) {
		scrambler_out = ( (scrambler >> 1) ^ scrambler) & 0x1;

		/* modify i-th bit by xor-ing with scrambler output sequence */
		ibit = LSB2MSB(i);
		if ( scrambler_out ) {
			out[ibit] = -in[ibit];
		} else {
			out[ibit] = in[ibit];
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
		j = LSB2MSB( (b * n) % SYMBOLS_PER_PACKET);
		out[i] = in[j];
	}
}


/* LDPC decode */
void horus_ldpc_decode(uint8_t *payload, float *sd) {
	float sum, mean, sign, sumsq, estvar, estEsN0, x;
	float llr[HORUS_SSDV_NUM_BITS];
	float temp[HORUS_SSDV_NUM_BITS];
	int i, parityCC;
	struct LDPC ldpc;

	/* normalise bitstream to log-like */
	sum = 0.0;
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ )
		sum += fabs(sd[i]);
	mean = sum / HORUS_SSDV_NUM_BITS;

	sum = sumsq = 0.0;
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ ) {
		sign = (sd[i] > 0.0) - (sd[i] < 0.0);
		x = (sd[i] / mean - sign);
		sum += x;
		sumsq += x * x;
	}
	x = HORUS_SSDV_NUM_BITS;
	estvar = (x * sumsq - sum * sum) / (x * (x - 1) );
	estEsN0 = 1.0 / (2.0 * estvar + 1E-3);
	for ( i = 0; i < HORUS_SSDV_NUM_BITS; i++ )
		llr[i] = 4.0 * estEsN0 * sd[i];

	/* remove unique word and re-order bits */
	unscramble(&llr[32], temp);
	deinterleave(temp,llr);

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

	i = run_ldpc_decoder(&ldpc, payload, llr, &parityCC);
}
