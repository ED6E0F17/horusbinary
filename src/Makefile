#gcc -lm -o horus_demod horus_demod.c horus_api.c horus_l2.c golay23.c fsk.c kiss_fft.c
CC=gcc
CFLAGS= -O3 -Wall
CFLAGS+= -DHORUS_L2_RX -DINTERLEAVER -DSCRAMBLER -DRUN_TIME_TABLES

all: clean horus_gateway horus_demod

horus_demod: horus_demod.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o
	g++  -lm -o horus_demod horus_demod.o horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o

.c.o:	$(CC)  $(CFLAGS) -c $< -o $@

clean:
	rm -f horus_demod horus_gateway *.o 

horus_gateway: gateway.o hiperfifo.o habitat.o utils.o utils.h global.h horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o mpdecode.o phi0.o
	g++ -o gateway gateway.o hiperfifo.o habitat.o utils.o  horus_api.o horus_l2.o golay23.o fsk.o kiss_fft.o ldpc.o  mpdecode.o phi0.o -lm -lcurl -lncurses

