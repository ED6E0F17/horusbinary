#gcc -lm -o horus_demod horus_demod.c horus_api.c horus_l2.c golay23.c fsk.c kiss_fft.c
CC=gcc
CFLAGS= -O3 -Wall
CFLAGS+= -DHORUS_L2_RX -DINTERLEAVER -DSCRAMBLER -DRUN_TIME_TABLES

all:   clean horus_gateway horus_demod ldpc_enc ldpc_dec ldpc_noise

horus_demod: horus_demod.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o
	g++  -lm -o horus_demod horus_demod.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o

.c.o:	$(CC)  $(CFLAGS) -c $< -o $@

clean:
	rm -f horus_demod horus_gateway *.o 

horus_gateway: gateway.o hiperfifo.o habitat.o utils.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o
	g++ -o gateway gateway.o hiperfifo.o habitat.o utils.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o -lm -lcurl -lncurses

ldpc_enc:  ldpc_enc.o mpdecode.o phi0.o predict.o
	g++ -o ldpc_enc ldpc_enc.o mpdecode.o phi0.o predict.o -lm

ldpc_dec:  ldpc_dec.o mpdecode.o phi0.o predict.o
	g++ -o ldpc_dec ldpc_dec.o mpdecode.o phi0.o predict.o -lm

ldpc_noise:  ldpc_noise.o
	g++ -o ldpc_noise ldpc_noise.o -lm

