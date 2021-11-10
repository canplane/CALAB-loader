all: apager2 test

apager2: apager2.c common.c
	gcc -static -Wl,-Ttext-segment=0x30000000 apager2.c -o apager2

test: test.c
	gcc -static test.c -o test

clean:
	rm -f apager2
	rm -f test
