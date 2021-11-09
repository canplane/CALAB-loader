all: apager dpager

apager: apager.c
	cc -Wall -Wl,-Ttext-segment=0x30000000 apager.c -o apager

dpager: dpager.c
	cc dpager.c -o dpager

test: test.c
	cc -static test.c -o test

clean:
	rm -f apager
	rm -f dpager
	rm -f test
