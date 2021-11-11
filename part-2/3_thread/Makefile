all: apager test

apager: apager.c common.c queue.c
	gcc -static -Wl,-Ttext-segment=0x50400000 apager.c -o apager

test: branch.c test.c
	gcc -static -Wl,-Ttext-segment=0x00400000 test.c -o test1
	gcc -static -Wl,-Ttext-segment=0x10400000 test.c -o test2
	gcc -static -Wl,-Ttext-segment=0x20400000 test.c -o test3
	gcc -static -Wl,-Ttext-segment=0x30400000 test.c -o test4

clean:
	rm -f apager
	rm -f test1
	rm -f test2
	rm -f test3
	rm -f test4
