all: apager dpager test

apager: apager.c common.c
	gcc -static -Wl,-Ttext-segment=0x30000000 apager.c -o apager

dpager: dpager.c common.c
	gcc -static -Wl,-Ttext-segment=0x30000000 dpager.c -o dpager

test: test_1.c test_2.c
	gcc -static test_1.c -o test_1
	gcc -static test_2.c -o test_2

clean:
	rm -f apager
	rm -f dpager
	rm -f test_1
	rm -f test_2
