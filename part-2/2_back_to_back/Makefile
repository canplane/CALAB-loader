all: apager test

apager: apager.c common.c queue.c
	gcc -static -Wl,-Ttext-segment=0x50000000 apager.c -o apager

test: branch.c test.c
	gcc -static test.c -o test

clean:
	rm -f apager
	rm -f test
