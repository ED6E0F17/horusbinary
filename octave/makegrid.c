/*
 * Generate a Matrix for ldpc in "H_rows" format.
 *
 * 	gcc -Wall makegrid.c -o makegrid
 */

#include <stdio.h>
#include <stdint.h>

#define DATBITS 128
#define PARBITS (DATBITS * 2)	/* 1/3 code rate */
#define DEPTH 2
#define PRIMES {13,47};

int16_t cols[DATBITS][DEPTH * 2];

void fillcols(uint16_t row, uint16_t col) {
	int i;

	for (i = 0; i < 2 * DEPTH; i++) {
		if (!cols[col][i]) {
			cols[col][i] = row + 1;
			return;
		}
	}
	printf("Failed !\n");
	return;
}

int main(void) {
	int b,d,i;
	int16_t prime[DEPTH] = PRIMES;

	for (b=0; b < DATBITS; b++) 
		for (d=0; d < DEPTH * 2; d++) 
			cols[b][d] = 0;

	printf("/* H_rows (128,384) Parity matrix for LDPC */\n");
	printf("#define COPRIME         337\n");
	printf("#define DATA_BYTES       16\n");
	printf("#define PARITY_BYTES     32\n");
	printf("#define CODELENGTH      384\n");
	printf("#define NUMBERPARITYBITS 256\n");
	printf("#define MAX_ROW_WEIGHT    2\n");
	printf("#define NUMBERROWSHCOLS 128\n");
	printf("#define MAX_COL_WEIGHT    4\n");
	printf("#define DEC_TYPE          0\n");
	printf("#define MAX_ITER        100\n");
	printf("\n// Autogenerated by prime numbers:\n//\t");
	for (b=0; b < DEPTH; b++)
		printf("%d;",prime[b]);

	printf("\n\nconst uint16_t H_rows[%d*%d] = {", PARBITS, DEPTH);
	for (d=0; d < DEPTH; d++) {
		printf("\n\t");
		for (b=0; b < PARBITS; b++) {
			uint16_t offset;

			offset = (3 * (b * prime[d] + d)) % DATBITS;
			fillcols(b, offset);
			printf("%2d,", offset + 1);
			for (i=0; i<d; i++)
				if (offset ==  (3 * (b * prime[i] + i)) % DATBITS) {
					printf("\n\nDuplicate!\n");
	    				return -1;
				}
		}
	}	    

	printf("\n};\n\nconst uint16_t H_cols[%d*%d] = {", DATBITS, 2*DEPTH);
	for (d=0; d < 2*DEPTH; d++) {
		printf("\n\t");
		for (b=0; b < DATBITS; b++) {
			printf("%2d,", cols[b][d]);
		}
	}
	printf("\n};\n\n");
	return 0;
}	

