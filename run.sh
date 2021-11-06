#!/bin/sh

cc apager.c -o apager
cc test.c -o test
./apager test
