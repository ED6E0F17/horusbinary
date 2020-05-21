
#include <stdio.h>
#include <stdint.h>

#define DATBITS 128
#define PARBITS (DATBITS * 2)	/* 1/3 code rate */
#define DEPTH 3
#define PRIMES {5,23,37};

int16_t cols[DATBITS][DEPTH * 2];

void fillcols(uint16_t row, uint16_t col) {
	int i;

	for (i = 0; i < 2 * DEPTH; i++) {
		if (!cols[col][i]) {
			cols[col][i] = row + 1;
			return;
		}
	}
	printf("Fialed !\n");
	return;
}

int main(void) {
	int b,d;
	int16_t prime[DEPTH] = PRIMES;

	for (b=0; b < DATBITS; b++) 
		for (d=0; d < DEPTH * 2; d++) 
			cols[b][d] = 0;

	printf("\n\nuint16_t H_rows[%d*%d] = {", PARBITS, DEPTH);
	for (d=0; d < DEPTH; d++) {
	    printf("\n\t");
		for (b=0; b < PARBITS; b++) {
		uint16_t offset;

		offset = (3 * (b * prime[d] + d)) % DATBITS;
		fillcols(b, offset);
		printf("%2d,", offset + 1);
	    }
	}	    

	printf("\n};\n\nuint16_t H_cols[%d*%d] = {", DATBITS, 2*DEPTH);
	for (d=0; d < 2*DEPTH; d++) {
		printf("\n\t");
		for (b=0; b < DATBITS; b++) {
			printf("%2d,", cols[b][d]);
		}
	}
	printf("\n};\n\n");
	return 0;
}	
