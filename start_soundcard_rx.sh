#!/usr/bin/env sh
sox -q -d -r 48k -c 1 -t s16 - | ./gateway
