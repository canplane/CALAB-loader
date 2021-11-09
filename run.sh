#!/bin/sh

cc -Wall -Wl,-Ttext-segment=0x30000000 apager.c -o apager
cc -static test.c -o test
./apager test a b c\ d e
