/*
  FILE...: mpdecode_core.c
  AUTHOR.: Matthew C. Valenti, Rohit Iyer Seshadri, David Rowe
  CREATED: Sep 2016

  C-callable core functions moved from MpDecode.c, so they can be used for
  Octave and C programs.
*/

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "mpdecode.h"
#include "phi0.h"

static int verbose;
void verbose_ldpc(int level) {verbose = level;}
#define  may_printf(X,Y) if(verbose)fprintf(stderr,X,Y)

// c_nodes will be an array of NumberParityBits of struct c_node
// Each c_node contains an array of <degree> c_sub_node elements
// This structure reduces the indexing caluclations in SumProduct()

struct c_sub_node { // Order is important here to keep total size small.
	uint16_t index; // Values from H_rows (except last 2 entries)
	uint16_t socket; // The socket number at the v_node
	float message;  // modified during operation!
};

struct c_node {
	int degree;     // A count of elements in the following arrays
	struct c_sub_node *subs;
};

// v_nodes will be an array of CodeLength of struct v_node

struct v_sub_node {
	uint16_t index; //    the index of a c_node it is connected to
	                //    Filled with values from H_cols (except last 2 entries)
	uint16_t socket; //    socket number at the c_node
	float message; //    Loaded with input data
	               //    modified during operation!
	uint8_t sign;  //    1 if input is negative
	               //    modified during operation!
};

struct v_node {
	int degree;     // A count of ???
	float initial_value;
	struct v_sub_node *subs;
};

void encode(struct LDPC *ldpc, const uint8_t ibits[], unsigned char pbits[]) {
	unsigned int p, i, tmp, par, prev = 0;
	int ind;
	const uint16_t *H_rows = ldpc->H_rows;

	for ( p = 0; p < ldpc->NumberParityBits; p++ ) {
		par = 0;

		for ( i = 0; i < ldpc->max_row_weight; i++ ) {
			ind = H_rows[p + i * ldpc->NumberParityBits];
            if (!ind)
                    continue;
			par = par + ibits[ind - 1];
		}

		tmp = par + prev;

		tmp &= 1;    // only retain the lsb
		prev = tmp;
		pbits[p] = tmp;
	}
}

/* Values for linear approximation (DecoderType=5) */

#define AJIAN -0.24904163195436
#define TJIAN 2.50681740420944

void init_c_v_nodes(struct c_node *c_nodes,
					int shift,
					int NumberParityBits,
					int max_row_weight,
					const uint16_t *H_rows,
					int H1,
					int CodeLength,
					struct v_node *v_nodes,
					int NumberRowsHcols,
					const uint16_t *H_cols,
					int max_col_weight,
					int dec_type,
					float  *input){
	int i, j, k, count, cnt, c_index, v_index;

	/* first determine the degree of each c-node */

	if ( shift == 0 ) {
		for ( i = 0; i < NumberParityBits; i++ ) {
			count = 0;
			for ( j = 0; j < max_row_weight; j++ ) {
				if ( H_rows[i + j * NumberParityBits] > 0 ) {
					count++;
				}
			}
			c_nodes[i].degree = count;
			if ( H1 ) {
				if ( i == 0 ) {
					c_nodes[i].degree = count + 1;
				} else {
					c_nodes[i].degree = count + 2;
				}
			}
		}
	} else {
		cnt = 0;
		for ( i = 0; i < (NumberParityBits / shift); i++ ) {
			for ( k = 0; k < shift; k++ ) {
				count = 0;
				for ( j = 0; j < max_row_weight; j++ ) {
					if ( H_rows[cnt + j * NumberParityBits] > 0 ) {
						count++;
					}
				}
				c_nodes[cnt].degree = count;
				if ( (i == 0) || (i == (NumberParityBits / shift) - 1) ) {
					c_nodes[cnt].degree = count + 1;
				} else {
					c_nodes[cnt].degree = count + 2;
				}
				cnt++;
			}
		}
	}

	if ( H1 ) {

		if ( shift == 0 ) {
			for ( i = 0; i < NumberParityBits; i++ ) {

				// Allocate sub nodes
				c_nodes[i].subs = CALLOC(c_nodes[i].degree, sizeof(struct c_sub_node) );
				assert(c_nodes[i].subs);

				// Populate sub nodes
				for ( j = 0; j < c_nodes[i].degree - 2; j++ ) {
					c_nodes[i].subs[j].index = (H_rows[i + j * NumberParityBits] - 1);
				}
				j = c_nodes[i].degree - 2;

				if ( i == 0 ) {
					c_nodes[i].subs[j].index = (H_rows[i + j * NumberParityBits] - 1);
				} else {
					c_nodes[i].subs[j].index = (CodeLength - NumberParityBits) + i - 1;
				}

				j = c_nodes[i].degree - 1;
				c_nodes[i].subs[j].index = (CodeLength - NumberParityBits) + i;

			}
		}
		if ( shift > 0 ) {
			cnt = 0;
			for ( i = 0; i < (NumberParityBits / shift); i++ ) {

				for ( k = 0; k < shift; k++ ) {

					// Allocate sub nodes
					c_nodes[cnt].subs = CALLOC(c_nodes[cnt].degree, sizeof(struct c_sub_node) );
					assert(c_nodes[cnt].subs);

					// Populate sub nodes
					for ( j = 0; j < c_nodes[cnt].degree - 2; j++ ) {
						c_nodes[cnt].subs[j].index = (H_rows[cnt + j * NumberParityBits] - 1);
					}
					j = c_nodes[cnt].degree - 2;
					if ( (i == 0) || (i == (NumberParityBits / shift - 1) ) ) {
						c_nodes[cnt].subs[j].index = (H_rows[cnt + j * NumberParityBits] - 1);
					} else {
						c_nodes[cnt].subs[j].index = (CodeLength - NumberParityBits) + k + shift * (i);
					}
					j = c_nodes[cnt].degree - 1;
					c_nodes[cnt].subs[j].index = (CodeLength - NumberParityBits) + k + shift * (i + 1);
					if ( i == (NumberParityBits / shift - 1) ) {
						c_nodes[cnt].subs[j].index = (CodeLength - NumberParityBits) + k + shift * (i);
					}
					cnt++;
				}
			}
		}

	} else {
		for ( i = 0; i < NumberParityBits; i++ ) {
			// Allocate sub nodes
			c_nodes[i].subs = CALLOC(c_nodes[i].degree, sizeof(struct c_sub_node) );
			assert(c_nodes[i].subs);

			// Populate sub nodes
			for ( j = 0; j < c_nodes[i].degree; j++ ) {
				c_nodes[i].subs[j].index = (H_rows[i + j * NumberParityBits] - 1);
			}
		}
	}


	/* determine degree of each v-node */

	for ( i = 0; i < (CodeLength - NumberParityBits + shift); i++ ) {
		count = 0;
		for ( j = 0; j < max_col_weight; j++ ) {
			if ( H_cols[i + j * NumberRowsHcols] > 0 ) {
				count++;
			}
		}
		v_nodes[i].degree = count;
	}

	for ( i = CodeLength - NumberParityBits + shift; i < CodeLength; i++ ) {
		count = 0;
		if ( H1 ) {
			if ( i != CodeLength - 1 ) {
				v_nodes[i].degree = 2;
			}  else {
				v_nodes[i].degree = 1;
			}

		} else {
			for ( j = 0; j < max_col_weight; j++ ) {
				if ( H_cols[i + j * NumberRowsHcols] > 0 ) {
					count++;
				}
			}
			v_nodes[i].degree = count;
		}
	}

	if ( shift > 0 ) {
		v_nodes[CodeLength - 1].degree = v_nodes[CodeLength - 1].degree + 1;
	}


	/* set up v_nodes */

	for ( i = 0; i < CodeLength; i++ ) {
		// Allocate sub nodes
		v_nodes[i].subs = CALLOC(v_nodes[i].degree, sizeof(struct v_sub_node) );
		assert(v_nodes[i].subs);

		// Populate sub nodes

		/* index tells which c-nodes this v-node is connected to */
		v_nodes[i].initial_value = input[i] * 0.99f;
		count = 0;

		for ( j = 0; j < v_nodes[i].degree; j++ ) {
			if ( (H1) && (i >= CodeLength - NumberParityBits + shift) ) {
				v_nodes[i].subs[j].index = i - (CodeLength - NumberParityBits + shift) + count;
				if ( shift == 0 ) {
					count = count + 1;
				} else {
					count = count + shift;
				}
			} else  {
				v_nodes[i].subs[j].index = (H_cols[i + j * NumberRowsHcols] - 1);
			}

			/* search the connected c-node for the proper message value */
			for ( c_index = 0; c_index < c_nodes[ v_nodes[i].subs[j].index ].degree; c_index++ )
				if ( c_nodes[ v_nodes[i].subs[j].index ].subs[c_index].index == i ) {
					v_nodes[i].subs[j].socket = c_index;
					break;
				}
			/* initialize v-node with received LLR */
			if ( dec_type == 1 ) {
				v_nodes[i].subs[j].message = fabs(input[i]);
			} else {
				v_nodes[i].subs[j].message = phi0( fabs(input[i]) );
			}

			if ( input[i] < 0 ) {
				v_nodes[i].subs[j].sign = 1;
			}
		}

	}



	/* now finish setting up the c_nodes */
	for ( i = 0; i < NumberParityBits; i++ ) {
		/* index tells which v-nodes this c-node is connected to */
		for ( j = 0; j < c_nodes[i].degree; j++ ) {
			/* search the connected v-node for the proper message value */
			for ( v_index = 0; v_index < v_nodes[ c_nodes[i].subs[j].index ].degree; v_index++ )
				if ( v_nodes[ c_nodes[i].subs[j].index ].subs[v_index].index == i ) {
					c_nodes[i].subs[j].socket = v_index;
					break;
				}
		}
	}

}


///////////////////////////////////////
/* function for doing the MP decoding */
// Returns the iteration count
int SumProduct( int       *parityCheckCount,
				char DecodedBits[],
				struct c_node c_nodes[],
				struct v_node v_nodes[],
				int CodeLength,
				int NumberParityBits,
				int max_iter){
	int result;
	int i,j, iter;
	float phi_sum;
	int sign;
	float temp_sum;
	float Qi;
	int ssum;
	int firstrun;

	may_printf("  %s","Bad parity bits:");
	firstrun = 1;

	result = max_iter;
	for ( iter = 0; iter < max_iter; iter++ ) {

		for ( i = 0; i < CodeLength; i++ ) DecodedBits[i] = 0; // Clear each pass!

		/* update r */
		ssum = 0;
		for ( j = 0; j < NumberParityBits; j++ ) {
			sign = v_nodes[ c_nodes[j].subs[0].index ].subs[ c_nodes[j].subs[0].socket ].sign;
			phi_sum = v_nodes[ c_nodes[j].subs[0].index ].subs[ c_nodes[j].subs[0].socket ].message;

			for ( i = 1; i < c_nodes[j].degree; i++ ) {
				// Compiler should optomize this but write the best we can to start from.
				struct c_sub_node *cp = &c_nodes[j].subs[i];
				struct v_sub_node *vp = &v_nodes[ cp->index ].subs[ cp->socket ];
				phi_sum += vp->message;
				sign ^= vp->sign;
			}

			if ( sign == 0 ) {
				ssum++;
			}

			for ( i = 0; i < c_nodes[j].degree; i++ ) {
				struct c_sub_node *cp = &c_nodes[j].subs[i];
				struct v_sub_node *vp = &v_nodes[ cp->index ].subs[ cp->socket ];
				if ( sign ^ vp->sign ) {
					cp->message = -phi0( phi_sum - vp->message ); // *r_scale_factor;
				} else {
					cp->message =  phi0( phi_sum - vp->message );// *r_scale_factor;
				}
			}
		}

		/* update q */
		for ( i = 0; i < CodeLength; i++ ) {
			/* first compute the LLR */
			Qi = v_nodes[i].initial_value;
			for ( j = 0; j < v_nodes[i].degree; j++ ) {
				struct v_sub_node *vp = &v_nodes[i].subs[j];
				Qi += c_nodes[ vp->index ].subs[ vp->socket ].message;
			}

			/* make hard decision */
			if ( Qi < 0 ) {
				DecodedBits[i] = 1;
			}

			/* now subtract to get the extrinsic information */
			for ( j = 0; j < v_nodes[i].degree; j++ ) {
				struct v_sub_node *vp = &v_nodes[i].subs[j];
				temp_sum = Qi - c_nodes[ vp->index ].subs[ vp->socket ].message;

				vp->message = phi0( fabs( temp_sum ) ); // *q_scale_factor;
				if ( temp_sum > 0 ) {
					vp->sign = 0;
				} else {
					vp->sign = 1;
				}
			}
		}

        if ((verbose > 1) || firstrun) {
            firstrun = 0;
            may_printf("%2d,", NumberParityBits - ssum);
		}

		// count the number of PC satisfied and exit if all OK
		*parityCheckCount = ssum;
		if ( ssum == NumberParityBits ) {
			result = iter + 1;
			break;
		}
	}

    if (verbose > 1) {
	    may_printf(" %2d iterations", iter);
    } else {
        may_printf(" Took %d iterations\n", iter);
    }

    return(result);
}


/* Convenience function to call LDPC decoder from C programs */

int run_ldpc_decoder(struct LDPC *ldpc, uint8_t out_char[], float input[], int *parityCheckCount) {
	int max_iter, dec_type;
	int max_row_weight, max_col_weight;
	int CodeLength, NumberParityBits, NumberRowsHcols, shift, H1;
	int i;
	struct c_node *c_nodes;
	struct v_node *v_nodes;

	/* default values */

	max_iter  = ldpc->max_iter;
	dec_type  = ldpc->dec_type;
	CodeLength = ldpc->CodeLength;                    /* length of entire codeword */
	NumberParityBits = ldpc->NumberParityBits;
	NumberRowsHcols = ldpc->NumberRowsHcols;

	char *DecodedBits = CALLOC( CodeLength, sizeof( char ) );
	assert(DecodedBits);

	/* derive some parameters */
	int DataLength = CodeLength - NumberParityBits;
	shift = NumberRowsHcols - DataLength;
	if ( NumberRowsHcols == CodeLength ) {
		H1 = 0;
		shift = 0;
	} else {
		H1 = 1;
	}

	max_row_weight = ldpc->max_row_weight;
	max_col_weight = ldpc->max_col_weight;

	/* initialize c-node and v-node structures */
	c_nodes = CALLOC( NumberParityBits, sizeof( struct c_node ) );
	assert(c_nodes);
	v_nodes = CALLOC( CodeLength, sizeof( struct v_node) );
	assert(v_nodes);

	init_c_v_nodes(c_nodes, shift, NumberParityBits, max_row_weight, ldpc->H_rows, H1, CodeLength,
				   v_nodes, NumberRowsHcols, ldpc->H_cols, max_col_weight, dec_type, input);
	for ( i = 0; i < CodeLength; i++ )
		DecodedBits[i] = 0;

	/* Call function to do the actual decoding */
	int iter = SumProduct( parityCheckCount, DecodedBits, c_nodes, v_nodes,
						   CodeLength, NumberParityBits, max_iter);

	for ( i = 0; i < CodeLength; i++ )
		out_char[i] = DecodedBits[i];

	/* Clean up memory */
	FREE(DecodedBits);

	for ( i = 0; i < NumberParityBits; i++ )
		FREE( c_nodes[i].subs );
	FREE( c_nodes );

	for ( i = 0; i < CodeLength; i++ )
		FREE( v_nodes[i].subs);
	FREE( v_nodes );

	return iter;
}


void sd_to_llr(float llr[], double sd[], int n) {
	double sum, mean, sign, sumsq, estvar, estEsN0, x;
	int i;

	/* convert SD samples to LLRs -------------------------------*/

	sum = 0.0;
	for ( i = 0; i < n; i++ )
		sum += fabs(sd[i]);
	mean = sum / n;

	/* find variance from +/-1 symbol position */

	sum = sumsq = 0.0;
	for ( i = 0; i < n; i++ ) {
		sign = (sd[i] > 0.0L) - (sd[i] < 0.0L);
		x = (sd[i] / mean - sign);
		sum += x;
		sumsq += x * x;
	}
	estvar = (n * sumsq - sum * sum) / (n * (n - 1) );
	//fprintf(stderr, "mean: %f var: %f\n", mean, estvar);

	estEsN0 = 1.0 / (2.0L * estvar + 1E-3);
	for ( i = 0; i < n; i++ )
		llr[i] = 4.0L * estEsN0 * sd[i];
}

void ldpc_print_info(struct LDPC *ldpc) {
	fprintf(stderr, "ldpc->max_iter = %d\n", ldpc->max_iter);
	fprintf(stderr, "ldpc->dec_type = %d\n", ldpc->dec_type);
	fprintf(stderr, "ldpc->q_scale_factor = %d\n", ldpc->q_scale_factor);
	fprintf(stderr, "ldpc->r_scale_factor = %d\n", ldpc->r_scale_factor);
	fprintf(stderr, "ldpc->CodeLength = %d\n", ldpc->CodeLength);
	fprintf(stderr, "ldpc->NumberParityBits = %d\n", ldpc->NumberParityBits);
	fprintf(stderr, "ldpc->NumberRowsHcols = %d\n", ldpc->NumberRowsHcols);
	fprintf(stderr, "ldpc->max_row_weight = %d\n", ldpc->max_row_weight);
	fprintf(stderr, "ldpc->max_col_weight = %d\n", ldpc->max_col_weight);
	fprintf(stderr, "ldpc->data_bits_per_frame = %d\n", ldpc->data_bits_per_frame);
	fprintf(stderr, "ldpc->coded_bits_per_frame = %d\n", ldpc->coded_bits_per_frame);
	fprintf(stderr, "ldpc->coded_syms_per_frame = %d\n", ldpc->coded_syms_per_frame);
}

/* vi:set ts=4 et sts=4: */
