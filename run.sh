#!/bin/sh

cc apager.c -o apager.out
cc -static test.c -o test.out
./apager.out test.out
