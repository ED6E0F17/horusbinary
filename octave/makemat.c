/* generate a matrix for octave -
 * use "sort -n" to get points in order, and add a header
 */

#include <stdio.h>
#include <stdint.h>

#define DATBITS 128
#define PARBITS (DATBITS * 2)	/* 1/3 code rate */
#define DEPTH 4
#define PRIMES {5,23,37,103};

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

	for (d=0; d < DEPTH; d++) {
		for (b=0; b < PARBITS; b++) {
			uint16_t offset;

			offset = (3 * (b * prime[d] + d)) % DATBITS;
			for (i=0; i<d; i++)
				if (offset ==  (3 * (b * prime[i] + i)) % DATBITS) {
					printf("\n\nDuplicate!\n");
	    				return -1;
				}
			printf("%03d %03d 1\n", b+1, offset+1);
		}
	}
	return 0;
}

