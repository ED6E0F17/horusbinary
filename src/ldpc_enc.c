/* 
  FILE...: ldpc_enc.c
  AUTHOR.: Bill Cowley, David Rowe
  CREATED: Sep 2016

  RA LDPC encoder program. Using the elegant back substitution of RA
  LDPC codes.

  building: gcc ldpc_enc.c -o ldpc_enc -Wall -g
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "mpdecode.h"
#include "HRA128_384.h"

int opt_exists(char *argv[], int argc, char opt[]) {
    int i;
    for (i=0; i<argc; i++) {
        if (strcmp(argv[i], opt) == 0) {
            return i;
        }
    }
    return 0;
}

/*
 * Pseudo-random number generator that we can implement in C with
 * identical results to Octave.  Returns an unsigned int between 0
 * and 32767.  Used for generating test frames of various lengths.
 */
void ofdm_rand(uint16_t r[], int n) {
    uint64_t seed = 1;
    int i;

    for (i = 0; i < n; i++) {
        seed = (1103515245l * seed + 12345) % 32768;
        r[i] = seed;
    }
}

int main(int argc, char *argv[])
{
    FILE         *fout;
    int           i, arg, frames, Nframes, data_bits_per_frame, parity_bits_per_frame;
    struct LDPC   ldpc;
    

    /* set up LDPC code from include file constants */

        ldpc.CodeLength = CODELENGTH;
        ldpc.NumberParityBits = NUMBERPARITYBITS;
        ldpc.NumberRowsHcols = NUMBERROWSHCOLS;
        ldpc.max_row_weight = MAX_ROW_WEIGHT;
        ldpc.max_col_weight = MAX_COL_WEIGHT;
        ldpc.H_rows = H_rows;
        ldpc.H_cols = H_cols;

    data_bits_per_frame = ldpc.NumberRowsHcols;
    parity_bits_per_frame = ldpc.NumberParityBits;
    
    unsigned char ibits[data_bits_per_frame];
    unsigned char pbits[parity_bits_per_frame];
    double        sdout[data_bits_per_frame+parity_bits_per_frame];

	    
    fout = stdout;
    Nframes = 100;

    if ((arg = (opt_exists(argv, argc, "--testframes")))) {
        Nframes = atoi(argv[arg+1]);
        fprintf(stderr, "Nframes: %d\n", Nframes);
    }

    frames = 0;
    for (i=0; i<data_bits_per_frame; i++)
    	ibits[i] = 0;
    encode(&ldpc, ibits, pbits);  
 
    while (frames < Nframes) {{
            /* map to BPSK symbols */
            for (i=0; i<data_bits_per_frame; i++)
                sdout[i] = 1.0 - 2.0 * ibits[i];
            for (i=0; i<parity_bits_per_frame; i++)
                sdout[i+data_bits_per_frame] = 1.0 - 2.0 * pbits[i];
        }
	fwrite(sdout, sizeof(double), data_bits_per_frame+parity_bits_per_frame, fout);
        frames++;       
    }

    return 0;
}
