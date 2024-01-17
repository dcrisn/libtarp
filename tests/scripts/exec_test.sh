#!/bin/bash

SLEEP_DURATION=2

# print environment variables; first 2 to stdout, next 2 to stderr
# this is expected by the main test program (see cexec.c)
# disable buffering so the prints come out in order!
stdbuf -o0 echo "$one"
stdbuf -o0 echo "$two"
stdbuf -e0 echo "$three" >&2
stdbuf -e0 echo "$four" >&2

sleep $SLEEP_DURATION
