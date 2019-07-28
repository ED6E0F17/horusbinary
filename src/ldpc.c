#include <stdint.h>
#include "math.h"
#include "mpdecode_core.h"
#include "H2064_516_sparse.h"  

#define BYTES_PER_PACKET       256
#define CRC_BYTES              2
#define PARITY_BYTES           65
#define BITS_PER_BYTE          10
#define UNPACKED_PACKET_BYTES  ((UW_BYTES+BYTES_PER_PACKET+CRC_BYTES)*BITS_PER_BYTE)
#define SYMBOLS_PER_PACKET     (BYTES_PER_PACKET+CRC_BYTES+PARITY_BYTES)*BITS_PER_BYTE
#define HORUS_SSDV_NUM_BITS    2616    /* image data (32 + (258 + 65) * 8) */


/* LDPC decode */
void horus_ldpc_decode(uint8_t *payload, float *sd) {
    float sum, mean, sign, sumsq, estvar, estEsN0, x;
    float llr[HORUS_SSDV_NUM_BITS];
    int i, parityCC;
    struct LDPC ldpc;

    sum = 0.0;
    for(i=0; i<HORUS_SSDV_NUM_BITS; i++)
        sum += fabs(sd[i]);
    mean = sum / HORUS_SSDV_NUM_BITS;

    sum = sumsq = 0.0;
    for(i=0; i<HORUS_SSDV_NUM_BITS; i++) {
        sign = (sd[i] > 0.0) - (sd[i] < 0.0);
        x = (sd[i]/mean - sign);
        sum += x;
        sumsq += x*x;
    }
    x = HORUS_SSDV_NUM_BITS;
    estvar = (x * sumsq - sum * sum) / (x * (x - 1));
    estEsN0 = 1.0/(2.0 * estvar + 1E-3);
    for(i=0; i<HORUS_SSDV_NUM_BITS; i++)
        llr[i] = 4.0 * estEsN0 * sd[i];

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

    i = run_ldpc_decoder(&ldpc, payload, &llr[32], &parityCC);
}


