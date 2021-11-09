all: apager dpager test

apager: apager.c
	gcc -static -Wl,-Ttext-segment=0x20000000 apager.c -o apager

dpager: dpager.c
	gcc -static -Wl,-Ttext-segment=0x20000000 dpager.c -o dpager

test: test.c
	gcc -static test.c -o test

clean:
	rm apager
	rm dpager
	rm test
