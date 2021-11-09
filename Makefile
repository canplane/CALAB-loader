all: test apager dpager

apager: apager.c
	gcc -static -Wl,-Ttext-segment=0x20000000 -o apager.c apager

dpager: dpager.c
	gcc -static -Wl,-Ttext-segment=0x20000000 -o dpager.c dpager

test: test.c
	gcc -static -o test.c test

clean:
	rm -f apager
	rm -f dpager
	rm -f test
