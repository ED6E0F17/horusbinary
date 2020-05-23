/* 
  FILE...: ldpc_dec.c
  AUTHOR.: Matthew C. Valenti, Rohit Iyer Seshadri, David Rowe
  CREATED: Sep 2016

  Command line C LDPC decoder derived from MpDecode.c in the CML
  library.  Allows us to run the same decoder in Octave and C.  The
  code is defined by the parameters and array stored in the include
  file below, which can be machine generated from the Octave function
  ldpc_fsk_lib.m:ldpc_decode()

  The include file also contains test input/output vectors for the LDPC
  decoder for testing this program.

  Build:

    $ gcc -O2 -o ldpc_dec ldpc_dec.c mpdecode_core.c -Wall -lm -g

  Note: -O2 option was required to get identical results to MpDecode,
  which is also compiled with -O2.  Without it the number of bit errors
  between C and Octave was different, especially when the code did
  not converge and hit max_iters.

*/

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "mpdecode.h"

/* Machine generated consts, H_rows, H_cols, test input/output data to
   change LDPC code regenerate this file. */
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
    int         i, parityCheckCount;
    int         data_bits_per_frame;
    struct LDPC ldpc;
    int         iter, total_iters, total_frames;
    int         Tbits, Terrs, Tbits_raw, Terrs_raw;


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


    data_bits_per_frame = ldpc.NumberRowsHcols;
    unsigned char ibits[CODELENGTH];
    unsigned char *pbits = ibits + data_bits_per_frame;
    uint8_t out_char[CODELENGTH];

    total_iters = 0;
    Tbits = Terrs = Tbits_raw = Terrs_raw = 0;
    
    {
        FILE *fin = stdin;
        int   nread;

        total_frames = 0;

 	for(i=0; i<data_bits_per_frame; i++)
                ibits[i] = 0;
	encode(&ldpc, ibits, pbits);  
 
        double input_double[CODELENGTH];
        float  input_float[CODELENGTH];

        nread = CODELENGTH;
        while(fread(input_double, sizeof(double), nread, fin) == nread) {
                    char in_char;
                    for (i=0; i<CODELENGTH; i++) {
                        in_char = input_double[i] < 0;
                        if (in_char != ibits[i]) {
                            Terrs_raw++;
                        }
                        Tbits_raw++;
                    }

		sd_to_llr(input_float, input_double, CODELENGTH);
		iter = run_ldpc_decoder(&ldpc, out_char, input_float, &parityCheckCount);

	    total_iters += iter;
            total_frames += 1;

            // fwrite(out_char, sizeof(char), data_bits_per_frame, fout);

                for (i=0; i<data_bits_per_frame; i++) {
                    if (out_char[i] != ibits[i]) {
                        Terrs++;
                    }
                    Tbits++;
                }
        }
    }

    if (total_frames) {
	fprintf(stderr, "Average iters: %d / %d\n", total_iters / total_frames, MAX_ITER);

        fprintf(stderr, "Raw: %d err: %d, BER: %4.3f\n", Tbits_raw, Terrs_raw,
                (float)Terrs_raw/(Tbits_raw+1E-12));
        float coded_ber = (float)Terrs/(Tbits+1E-12);
        fprintf(stderr, "Out: %d err: %d, BER: %4.3f\n", Tbits, Terrs, coded_ber);

    } 
    return 0;
}


