all: apager test

apager: apager.c common.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

test: branch.c example/my_test.c example/bss.c example/func_call.c example/hello_world.c example/print.c example/test.c
	gcc -static -Wl,-Ttext-segment=0x10000000 example/my_test.c -o example/my_test
	gcc -static -Wl,-Ttext-segment=0x18000000 example/bss.c -o example/bss
	gcc -static -Wl,-Ttext-segment=0x20000000 example/func_call.c -o example/func_call
	gcc -static -Wl,-Ttext-segment=0x28000000 example/hello_world.c -o example/hello_world
	gcc -static -Wl,-Ttext-segment=0x30000000 example/print.c -o example/print
	gcc -static -Wl,-Ttext-segment=0x38000000 example/test.c -o example/test

run:
	./apager example/my_test example/bss example/func_call example/hello_world example/print example/test

clean:
	rm -f apager
	rm -f example/my_test
	rm -f example/bss
	rm -f example/func_call
	rm -f example/hello_world
	rm -f example/print
	rm -f example/test