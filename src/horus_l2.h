/*---------------------------------------------------------------------------*\

  FILE........: horus_l2.h
  AUTHOR......: David Rowe
  DATE CREATED: Dec 2015

\*---------------------------------------------------------------------------*/

#ifndef __HORUS_L2__
#define __HORUS_L2__

/* call this first */
void horus_l2_init(void);

/* returns number of output bytes in output_tx_data */
int horus_l2_get_num_tx_data_bytes(int num_payload_data_bytes);


/* returns number of output bytes in output_tx_data */
int horus_l2_encode_tx_packet(unsigned char *output_tx_data,
                              unsigned char *input_payload_data,
                              int            num_payload_data_bytes);

void horus_l2_decode_rx_packet(unsigned char *output_payload_data,
                               unsigned char *input_rx_data,
                               int            num_payload_data_bytes);

unsigned short horus_l2_gen_crc16(unsigned char* data_p,
				  unsigned char length);

void horus_ldpc_decode(uint8_t *payload, float *sd); 
void ldpc_errors(const uint8_t *packet, uint8_t *rx_bytes);
void set_error_count(int percentage);
void interleave(unsigned char *inout, int nbytes, int dir);
void scramble(unsigned char *inout, int nbytes);
void confirm_good(int ok);
#endif
