#!/usr/bin/env sh
echo "Launching gateway with rtl_fm in raw IQ mode"
# tune 1000 HZ below expected lowest tone:
rtl_fm -M raw -s 48000 -p 0 -f 434415000 | ./gateway -q
