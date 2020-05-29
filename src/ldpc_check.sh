#! /bin/bash

# ldpc_check
#
# A series of checks of the ldpc functions, mostly decode.
# 
# This uses ldpc_enc to supply test data to ldpc_dec and mosty
# assumes that the encode function is correct.

# PATH
PATH=$PATH:../src
PASS=1

###############################
echo "Simple test, no added noise"
ldpc_enc --testframes 10 |
#    ldpc_noise - - -3.0 |
    ldpc_dec >/dev/null  2> tmp
cat tmp
n=$(grep 'BER: 0.000' tmp | wc -l)
if [ $n == 2 ]; then echo "OK"; else echo "BAD"; PASS=0; fi

###############################
echo "Simple test, default, 0.0 db noise"
ldpc_enc --testframes 1000 |
    ldpc_noise - - 0.0 2> /dev/null|
    ldpc_dec >/dev/null 2> tmp
cat tmp
n=$(grep 'Raw.*BER:' tmp | cut -d ' ' -f 6)
p1=$(echo $n '<=' 0.12 | bc)
n=$(grep 'Out.*BER:' tmp | cut -d ' ' -f 6)
p2=$(echo $n '<=' 0.01 | bc)
if [[ $p1 -eq 1 && $p2 -eq 1 ]]; then echo "OK"; else echo "BAD"; PASS=0; fi

##############################################################
echo "Simple test, default, 3.0 db noise"
ldpc_enc --testframes 2000 |
    ldpc_noise - - 3.0 2> /dev/null |
    ldpc_dec >/dev/null 2> tmp
cat tmp
n=$(grep 'Raw.*BER:' tmp | cut -d ' ' -f 6)
p1=$(echo $n '>=' 0.15 | bc)
n=$(grep 'Out.*BER:' tmp | cut -d ' ' -f 6)
p2=$(echo $n '<=' 0.05 | bc)
if [[ $p1 -eq 1 && $p2 -eq 1 ]]; then echo "OK"; else echo "BAD"; PASS=0; fi

##############################################################

if [[ $PASS == 1 ]]; then echo "PASSED"; else echo "FAILED"; fi
