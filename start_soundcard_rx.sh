#!/usr/bin/env sh
sox -d -r 48k -c 1 -t s16 - | ./gateway
