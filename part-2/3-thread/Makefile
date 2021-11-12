all: apager dpager test

apager: apager.c common.c queue.c debug.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

dpager: dpager.c common.c queue.c debug.c
	gcc -static -Wl,-Ttext-segment=0x60000000 dpager.c -o dpager

test: branch.c example_yield/a.c example_yield/b.c example_yield/c.c
	gcc -static -Wl,-Ttext-segment=0x10000000 example_yield/a.c -o example_yield/a
	gcc -static -Wl,-Ttext-segment=0x18000000 example_yield/b.c -o example_yield/b
	gcc -static -Wl,-Ttext-segment=0x20000000 example_yield/c.c -o example_yield/c

run_apager:
	./apager example_yield/a example_yield/b example_yield/c

run_dpager:
	./dpager example_yield/a example_yield/b example_yield/c

clean:
	rm -f apager
	rm -f dpager
	rm -f example_yield/a
	rm -f example_yield/b
	rm -f example_yield/c