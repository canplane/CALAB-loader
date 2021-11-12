all: apager dpager test

apager: apager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

dpager: dpager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 dpager.c -o dpager

test: example/my_test.c
	gcc -static example/my_test.c -o example/my_test

run_apager:
	./apager example/my_test

run_dpager:
	./dpager example/my_test

clean:
	rm -f apager
	rm -f dpager
	rm -f example/my_test
