all: apager test

apager: apager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

test: branch.c test.c
	gcc -static test.c -o test1
	gcc -static test.c -o test2
	gcc -static test.c -o test3
	gcc -static test.c -o test4
	gcc -static test.c -o test5
	gcc -static test.c -o test6
	gcc -static test.c -o test7
	gcc -static test.c -o test8

run:
	./apager test1 test2 test3 test4 test5 test6 test7 test8

clean:
	rm -f apager
	rm -f test1
	rm -f test2
	rm -f test3
	rm -f test4
	rm -f test5
	rm -f test6
	rm -f test7
	rm -f test8