all: apager dpager test

apager: apager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

dpager: dpager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 dpager.c -o dpager

test: branch.c example/my_test.c example/bss.c example/func_call.c example/hello_world.c example/print.c example/test.c
	gcc -static example/my_test.c -o example/my_test
	gcc -static example/bss.c -o example/bss
	gcc -static example/func_call.c -o example/func_call
	gcc -static example/hello_world.c -o example/hello_world
	gcc -static example/print.c -o example/print
	gcc -static example/test.c -o example/test

run_apager:
	./apager example/my_test example/bss example/func_call example/hello_world example/print example/test

run_dpager:
	./dpager example/my_test example/bss example/func_call example/hello_world example/print example/test

clean:
	rm -f apager
	rm -f dpager
	rm -f example/my_test
	rm -f example/bss
	rm -f example/func_call
	rm -f example/hello_world
	rm -f example/print
	rm -f example/test