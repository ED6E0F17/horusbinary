/*---------------------------------------------------------------------------*\

  FILE........: horus_api.c
  AUTHOR......: David Rowe
  DATE CREATED: March 2018

  Library of API functions that implement High Altitude Balloon (HAB)
  telemetry modems and protocols for Project Horus.  May also be useful for
  other HAB projects.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2018 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "horus_api.h"
#include "fsk.h"
#include "horus_l2.h"

#define MAX_UW_LENGTH                  100
#define HORUS_API_VERSION                1    /* unique number that is bumped if API changes */
#define HORUS_BINARY_NUM_BITS          376    /* Telemetry data (22 * 8 * 23/12 + 32)        */
#define HORUS_BINARY_NUM_PAYLOAD_BYTES  22    /* fixed number of bytes in binary payload     */
#define HORUS_BINARY_LONG_PAYLOAD_BYTES 32    /* extended binary payload */
#define HORUS_MAX_PAYLOAD_BYTES        255    /* image data or other long packet*/
#define HORUS_SSDV_NUM_BITS           3960    /* image data (255 * 8 * 23 / 12 + 32)    */
#define HORUS_BINARY_SAMPLERATE      48000    /* no reason to change this */
#define HORUS_BINARY_SYMBOLRATE        100    /* may be changed for larger packet sizes */
#define HORUS_SSDV_SYMBOLRATE         1000
#define HORUS_BINARY_TS              (HORUS_BINARY_SAMPLERATE / HORUS_BINARY_SYMBOLRATE)
#define HORUS_BINARY_NIN_MAX         (HORUS_BINARY_SAMPLERATE + HORUS_BINARY_TS * 2)
#define HORUS_MIN_SSDV_SPACING        1000
#define HORUS_MAX_FREQUENCY           7000

struct horus {
    int         mode;
    int         verbose;
    struct FSK *fsk;                 /* states for FSK modem                */
    int         Fs;                  /* sample rate in Hz                   */
    int         mFSK;                /* number of FSK tones                 */
    int         Rs;                  /* symbol rate in Hz                   */
    int         uw[MAX_UW_LENGTH];   /* unique word bits mapped to +/-1     */
    int         uw_thresh;           /* threshold for UW detection          */
    int         uw_len;              /* length of unique word               */
    int         max_packet_len;      /* max length of a telemetry packet    */
    uint8_t    *rx_bits;             /* buffer of received bits             */
    int         rx_bits_len;         /* length of rx_bits buffer            */
    int         crc_ok;              /* most recent packet checksum results */
    int         total_payload_bits;  /* num bits rx-ed in last RTTY packet  */
};

/* Unique word for Horus RTTY 7 bit '$' character, 3 sync bits,
   (2 stop and next start), repeated 2 times */

int8_t uw_horus_rtty[] = {
  0,0,1,0,0,1,0,1,1,0,
  0,0,1,0,0,1,0,1,1,0
};

/* Unique word for Horus Binary (<ESC><ESC>$$)
   - Horus payload sends 4 <ESC> chars as a preamble */

int8_t uw_horus_binary[] = {
    0,0,0,1,1,0,1,1,	// escape
    0,0,0,1,1,0,1,1,	// escape
    0,0,1,0,0,1,0,0,	// $
    0,0,1,0,0,1,0,0 	// $
};


struct horus *horus_open (int mode) {
    int i;
    assert((mode == HORUS_MODE_RTTY) || (mode == HORUS_MODE_BINARY) || (mode == HORUS_MODE_SSDV));

    struct horus *hstates = (struct horus *)malloc(sizeof(struct horus));
    assert(hstates != NULL);

    hstates->Fs = HORUS_BINARY_SAMPLERATE;
    if (mode == HORUS_MODE_SSDV)
        hstates->Rs = HORUS_SSDV_SYMBOLRATE;
    else
        hstates->Rs = HORUS_BINARY_SYMBOLRATE;
    hstates->verbose = 0;
    hstates->mode = mode;

    if (mode == HORUS_MODE_RTTY) {
        hstates->mFSK = 2;
        hstates->max_packet_len = 1000;

        /* map UW to make it easier to search for */

        for (i=0; i<sizeof(uw_horus_rtty); i++) {
            hstates->uw[i] = 2*uw_horus_rtty[i] - 1;
        }        
        hstates->uw_len = sizeof(uw_horus_rtty);
        hstates->uw_thresh = sizeof(uw_horus_rtty);	/* allow no bit errors in UW detection */
        hstates->rx_bits_len = hstates->max_packet_len;
    }

    if (mode == HORUS_MODE_BINARY) {
        hstates->mFSK = 4;
        hstates->max_packet_len = HORUS_BINARY_NUM_BITS;
        for (i=0; i<sizeof(uw_horus_binary); i++) {
            hstates->uw[i] = 2*uw_horus_binary[i] - 1;
        }
        hstates->uw_len = sizeof(uw_horus_binary);
        hstates->uw_thresh = sizeof(uw_horus_binary) - 6; /* allow 3 bit errors in UW detection */
        horus_l2_init();
        hstates->rx_bits_len = hstates->max_packet_len;
    }

    if (mode == HORUS_MODE_SSDV) {
        hstates->mFSK = 4;
        hstates->max_packet_len = HORUS_SSDV_NUM_BITS;
        for (i=0; i<sizeof(uw_horus_binary); i++) {
            hstates->uw[i] = 2*uw_horus_binary[i] - 1;
        }
        hstates->uw_len = sizeof(uw_horus_binary);
        hstates->uw_thresh = sizeof(uw_horus_binary) - 6; /* allow 3 bit errors in UW detection */
        horus_l2_init();
        hstates->rx_bits_len = hstates->max_packet_len;
    }

    hstates->fsk = fsk_create(hstates->Fs, hstates->Rs, hstates->mFSK, 1000, 2*hstates->Rs);
    if (mode == HORUS_MODE_SSDV) {
        hstates->fsk->est_max = HORUS_MAX_FREQUENCY;
        hstates->fsk->est_space = HORUS_MIN_SSDV_SPACING;
    }

    /* allocate enough room for two packets so we know there will be
       one complete packet if we find a UW at start */
    
    hstates->rx_bits_len += hstates->fsk->Nbits;
    hstates->rx_bits = (uint8_t*)malloc(hstates->rx_bits_len);
    assert(hstates->rx_bits != NULL);
    for(i=0; i<hstates->rx_bits_len; i++) {
        hstates->rx_bits[i] = 0;
    }


    hstates->crc_ok = 0;
    hstates->total_payload_bits = 0;
    
    return hstates;
}

void horus_close (struct horus *hstates) {
    assert(hstates != NULL);
    fsk_destroy(hstates->fsk);
    free(hstates->rx_bits);
    free(hstates);
}

uint32_t horus_nin(struct horus *hstates) {
    assert(hstates != NULL);
    int nin = fsk_nin(hstates->fsk);
    assert(nin <= horus_get_max_demod_in(hstates));
    return nin;
}

int horus_find_uw(struct horus *hstates, int n) {
    int i, j, corr, mx, mx_ind;
    int rx_bits_mapped[n+hstates->uw_len];
    
    /* map rx_bits to +/-1 for UW search */

    for(i=0; i<n+hstates->uw_len; i++) {
        rx_bits_mapped[i] = 2*hstates->rx_bits[i] - 1;
    }
    
    /* look for UW  */

    mx = 0; mx_ind = 0;
    for(i=0; i<n; i++) {

        /* calculate correlation between bit stream and UW */
        
        corr = 0;
        for(j=0; j<hstates->uw_len; j++) {
            corr += rx_bits_mapped[i+j]*hstates->uw[j];
        }
        
        /* peak pick maximum */
        
        if (corr > mx) {
            mx = corr;
            mx_ind = i;
        }
    }

    if (hstates->verbose) {
        fprintf(stderr, "  horus_find_uw: mx_ind: %d mx: %d uw_thresh: %d n: %d\n",  mx_ind, mx, hstates->uw_thresh, n);
    }
    
    if (mx >= hstates->uw_thresh) {
        return mx_ind;
    } else {
        return -1;
    }
}

int hex2int(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}


int extract_horus_rtty(struct horus *hstates, char ascii_out[], int uw_loc) {
    const int nfield = 7;                               /* 7 bit ASCII                    */
    const int npad   = 3;                               /* 3 sync bits between characters */
    int st = uw_loc;                                    /* first bit of first char        */
    int en = hstates->max_packet_len - nfield;          /* last bit of max length packet  */

    int      i, j, endpacket, nout, crc_ok;
    uint8_t  char_dec;
    char    *pout, *ptx_crc;
    uint16_t rx_crc, tx_crc;

    pout = ascii_out; nout = 0; crc_ok = 0; endpacket = 0; rx_crc = tx_crc = 0;
    
    for (i=st; i<en; i+=nfield+npad) {

        /* assemble char LSB to MSB */

        char_dec = 0;
        for(j=0; j<nfield; j++) {
            assert(hstates->rx_bits[i+j] <= 1);
            char_dec |= hstates->rx_bits[i+j] * (1<<j);
        }
        if (hstates->verbose) {
            fprintf(stderr, "  extract_horus_rtty i: %4d 0x%02x %c ", i, char_dec, char_dec);
            if ((nout % 6) == 0) {
                fprintf(stderr, "\n");
            }
        }

        /*  if we find a '*' that's the end of the packet for RX CRC calculations */

        if (!endpacket && (char_dec == 42)) {
            endpacket = 1;
            rx_crc = horus_l2_gen_crc16((uint8_t*)&ascii_out[5], nout-5);
            ptx_crc = pout + 1; /* start of tx CRC */
        }

        /* build up output array, really only need up to tx crc but
           may end up going further */
        
        *pout++ = (char)char_dec;
        nout++;
        
    }

    /* if we found the end of packet flag and have enough chars to compute checksum ... */

    //fprintf(stderr, "\n\ntx CRC...\n");
    if (endpacket && (pout > (ptx_crc+3))) {
        tx_crc = 0;
        for(i=0; i<4; i++) {
            tx_crc <<= 4;
            tx_crc |= hex2int(ptx_crc[i]);
            //fprintf(stderr, "ptx_crc[%d] %c 0x%02X tx_crc: 0x%04X\n", i, ptx_crc[i], hex2int(ptx_crc[i]), tx_crc);
        }
        crc_ok = (tx_crc == rx_crc);
        *(ptx_crc+4) = 0;  /* terminate ASCII string */

        if (crc_ok) {
            hstates->total_payload_bits = strlen(ascii_out)*7;
        }
    }
    else {
        *ascii_out = 0;
    }

    if (hstates->verbose) {
        fprintf(stderr, "\n  endpacket: %d nout: %d tx_crc: 0x%04x rx_crc: 0x%04x\n",
                endpacket, nout, tx_crc, rx_crc);
    }
            
    /* make sure we don't overrun storage */
    
    assert(nout <= horus_get_max_ascii_out_len(hstates));

    hstates->crc_ok = crc_ok;
    
    return crc_ok;
}


int extract_horus_binary(struct horus *hstates, char hex_out[], int uw_loc, int payload_size) {
    const int nfield = 8;                      /* 8 bit binary                   */
    int st = uw_loc;
    int en = uw_loc + hstates->max_packet_len; /* last bit of max length packet  */

    int      j, b, nout;
    uint8_t  rxpacket[hstates->max_packet_len];
    uint8_t  rxbyte, *pout;
 
    /* convert bits to a packet of bytes */
    
    pout = rxpacket; nout = 0;
    
    for (b=st; b<en; b+=nfield) {

        /* assemble bytes MSB to LSB */

        rxbyte = 0;
        for(j=0; j<nfield; j++) {
            assert(hstates->rx_bits[b+j] <= 1);
            rxbyte <<= 1;
            rxbyte |= hstates->rx_bits[b+j];
        }
        
        /* build up output array */
        
        *pout++ = rxbyte;
        nout++;
    }

    if (hstates->verbose) {
        fprintf(stderr, "  extract_horus_binary nout: %d\n  Received Packet before decoding:\n  ", nout);
        for (b=0; b<nout; b++) {
            fprintf(stderr, "%02X", rxpacket[b]);
        }
        fprintf(stderr, "\n");
    }
    
    uint8_t payload_bytes[HORUS_MAX_PAYLOAD_BYTES];
    horus_l2_decode_rx_packet(payload_bytes, rxpacket, payload_size);
    if (payload_size == HORUS_BINARY_NUM_PAYLOAD_BYTES) {
        uint16_t crc_tx, crc_rx;
        crc_rx = horus_l2_gen_crc16(payload_bytes, HORUS_BINARY_NUM_PAYLOAD_BYTES-2);
        crc_tx = (uint16_t)payload_bytes[HORUS_BINARY_NUM_PAYLOAD_BYTES-2] +
                ((uint16_t)payload_bytes[HORUS_BINARY_NUM_PAYLOAD_BYTES-1]<<8);
        hstates->crc_ok = (crc_tx == crc_rx);

        if (hstates->verbose) {
            fprintf(stderr, "  extract_horus_binary crc_tx: %04X crc_rx: %04X\n", crc_tx, crc_rx);
        }
    } else {
        uint32_t crc_tx, crc_rx;
        crc_rx = horus_l2_gen_crc32(payload_bytes, payload_size-4);
	/* SSDV compatible checksum is MSB */
        crc_tx = (uint32_t)payload_bytes[payload_size-1] +
                ((uint32_t)payload_bytes[payload_size-2]<<8) +
                ((uint32_t)payload_bytes[payload_size-3]<<16) +
                ((uint32_t)payload_bytes[payload_size-4]<<24);
        hstates->crc_ok = (crc_tx == crc_rx);
        if (hstates->verbose) {
            fprintf(stderr, "  extract_horus_binary crc_tx: %08X crc_rx: %08X\n", crc_tx, crc_rx);
	}
    }

    /* convert to ASCII string of hex characters */

    hex_out[0] = 0;
    char hex[3];
    for (b=0; b<payload_size; b++) {
        sprintf(hex, "%02X", payload_bytes[b]);
        strcat(hex_out, hex);
    }
   
    if (hstates->verbose) {
        fprintf(stderr, "  nout: %d Decoded Payload bytes:\n  %s\n", payload_size, hex_out);
    }

    /* With noise input to FSK demod we can get occasinal UW matches,
       so a good idea to only pass on any packets that pass CRC */
    
    if ( hstates->crc_ok) {
        hstates->total_payload_bits += payload_size;
    }
    return hstates->crc_ok;
}

int horus_rx(struct horus *hstates, char ascii_out[], short demod_in[]) {
    int i;
    COMP demod_in_comp[HORUS_BINARY_NIN_MAX];

    assert(hstates != NULL);

    for (i=0; i<hstates->fsk->nin; i++) {
        demod_in_comp[i].real = demod_in[i];
        demod_in_comp[i].imag = 0;
    }
    return horus_demod_comp(hstates, ascii_out, demod_in_comp);
}

int horus_rx_comp(struct horus *hstates, char ascii_out[], short demod_in_iq[]) {
    int i;
    COMP demod_in_comp[HORUS_BINARY_NIN_MAX];

    assert(hstates != NULL);

    for (i=0; i<hstates->fsk->nin; i++) {
        demod_in_comp[i].real = demod_in_iq[i * 2];     // cast shorts to floats
        demod_in_comp[i].imag = demod_in_iq[i * 2 + 1]; //  range not normalised
    }
    return horus_demod_comp(hstates, ascii_out, demod_in_comp);
}

/* Tracking for packets corrupt or misdetected */
static int found_uw = 0;
static int good_crc = 0;
int horus_bad_crc(void) {return found_uw - good_crc;}

int horus_demod_comp(struct horus *hstates, char ascii_out[], COMP demod_in_comp[]) {
    int i, j, uw_loc, packet_detected;
    
    packet_detected = 0;

    int Nbits = hstates->fsk->Nbits;
    int rx_bits_len = hstates->rx_bits_len;
    
    if (hstates->verbose) {
        fprintf(stderr, "  horus_rx max_packet_len: %d rx_bits_len: %d Nbits: %d nin: %d\n",
                hstates->max_packet_len, rx_bits_len, Nbits, hstates->fsk->nin);
    }
    
    /* shift buffer of bits to make room for new bits */

    for(i=0,j=Nbits; j<rx_bits_len; i++,j++) {
        hstates->rx_bits[i] = hstates->rx_bits[j];
    }

    /* demodulate latest bits */

    fsk_demod(hstates->fsk, &hstates->rx_bits[rx_bits_len-Nbits], demod_in_comp);

    /* UW search to see if we can find the start of a packet in the buffer */
    if ((uw_loc = horus_find_uw(hstates, Nbits)) != -1) {

        if (hstates->verbose) {
            fprintf(stderr, "  horus_rx uw_loc: %d mode: %d\n", uw_loc, hstates->mode);
        }
        
        /* OK we have found a unique word, and therefore the start of
           a packet, so lets try to extract valid packets */

        if (hstates->mode == HORUS_MODE_RTTY) {
            packet_detected = extract_horus_rtty(hstates, ascii_out, uw_loc);
        }
        if (hstates->mode == HORUS_MODE_BINARY) {
            packet_detected = extract_horus_binary(hstates, ascii_out, uw_loc, HORUS_BINARY_NUM_PAYLOAD_BYTES);
            //#define DUMP_BINARY_PACKET
            #ifdef DUMP_BINARY_PACKET
            FILE *f = fopen("packetbits.txt", "wt"); assert(f != NULL);
            for(i=0; i<hstates->max_packet_len; i++) {
                fprintf(f,"%d ", hstates->rx_bits[uw_loc+i]);
            }
            fclose(f);
            exit(0);
            #endif
        }
	/* Check for shorter binary packet first, then for longer image packet */
        if (hstates->mode == HORUS_MODE_SSDV) {
            packet_detected = extract_horus_binary(hstates, ascii_out, uw_loc, HORUS_BINARY_LONG_PAYLOAD_BYTES);
            if (!packet_detected)
                packet_detected = extract_horus_binary(hstates, ascii_out, uw_loc, HORUS_MAX_PAYLOAD_BYTES);
	}
	found_uw++;
    }

    if (packet_detected)
	    good_crc++;
    return packet_detected;
}

int horus_get_version(void) {
    return HORUS_API_VERSION;
}

int horus_get_mode(struct horus *hstates) {
    assert(hstates != NULL);
    return hstates->mode;
}

int horus_get_Fs(struct horus *hstates) {
    assert(hstates != NULL);
    return hstates->Fs;
}

int horus_get_mFSK(struct horus *hstates) {
    assert(hstates != NULL);
    return hstates->mFSK;
}

int horus_get_max_demod_in(struct horus *hstates) {
    /* sizeof(short) * (hstates->fsk->N + hstates->fsk->Ts*2) */
    return HORUS_BINARY_NIN_MAX * sizeof(short);
}

int horus_get_max_ascii_out_len(struct horus *hstates) {
    assert(hstates != NULL);
    if (hstates->mode == HORUS_MODE_RTTY) {
        return hstates->max_packet_len/10;     /* 7 bit ASCII, plus 3 sync bits */
    }
    if (hstates->mode == HORUS_MODE_BINARY) {
        return 2*HORUS_BINARY_NUM_PAYLOAD_BYTES+1; /* HEX DUMP */
    }
    if (hstates->mode == HORUS_MODE_SSDV) {
        return 2*HORUS_MAX_PAYLOAD_BYTES+1; /* HEX DUMP */
    }
    assert(0); /* should never get here */
    return 0;
}

void horus_get_modem_stats(struct horus *hstates, int *sync, float *snr_est) {
    struct MODEM_STATS stats;
    assert(hstates != NULL);

    /* TODO set sync if UW found "recently", but WTF is recently? Maybe need a little state 
       machine to "blink" sync when we get a packet */

    *sync = 0;
    
    /* SNR scaled from Eb/No est returned by FSK to SNR in 3000 Hz */

    fsk_get_demod_stats(hstates->fsk, &stats);
    *snr_est = stats.snr_est + 10*log10((float)hstates->Rs*log2(hstates->mFSK)/3000);
}

void horus_get_modem_extended_stats (struct horus *hstates, struct MODEM_STATS *stats) {
    int i;
    
    assert(hstates != NULL);

    fsk_get_demod_stats(hstates->fsk, stats);
    if (hstates->verbose) {
        fprintf(stderr, "  horus_get_modem_extended_stats stats->snr_est: %f\n", stats->snr_est);
    }
    stats->snr_est = stats->snr_est + 10*log10((float)hstates->Rs*log2(hstates->mFSK)/3000);

    assert(hstates->mFSK <= MODEM_STATS_MAX_F_EST);
    for (i=0; i<hstates->mFSK; i++) {
        stats->f_est[i] = hstates->fsk->f_est[i];
    }
}

void horus_set_verbose(struct horus *hstates, int verbose) {
    assert(hstates != NULL);
    hstates->verbose = verbose;
}

int horus_crc_ok(struct horus *hstates) {
    assert(hstates != NULL);
    return hstates->crc_ok;
}

int horus_get_total_payload_bits(struct horus *hstates) {
    assert(hstates != NULL);
    return hstates->total_payload_bits;
}

void horus_set_total_payload_bits(struct horus *hstates, int val) {
    assert(hstates != NULL);
    hstates->total_payload_bits = val;
}
