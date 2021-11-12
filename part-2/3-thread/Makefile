all: apager dpager test

apager: apager.c common.c queue.c debug.c
	gcc -static -Wl,-Ttext-segment=0x60000000 apager.c -o apager

dpager: dpager.c common.c queue.c debug.c
	gcc -static -Wl,-Ttext-segment=0x60000000 dpager.c -o dpager

test: branch.c ../example/yield/a.c ../example/yield/b.c ../example/yield/c.c
	gcc -static -Wl,-Ttext-segment=0x10000000 ../example/yield/a.c -o ../example/yield/a
	gcc -static -Wl,-Ttext-segment=0x18000000 ../example/yield/b.c -o ../example/yield/b
	gcc -static -Wl,-Ttext-segment=0x20000000 ../example/yield/c.c -o ../example/yield/c

run_apager:
	./apager ../example/yield/a ../example/yield/b ../example/yield/c

run_dpager:
	./dpager ../example/yield/a ../example/yield/b ../example/yield/c

clean:
	rm -f apager
	rm -f dpager
	rm -f ../example/yield/a
	rm -f ../example/yield/b
	rm -f ../example/yield/c