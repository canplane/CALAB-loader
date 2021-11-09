all: apager dpager hpager

apager: apager.c
	cc apager.c -o apager

dpager: dpager.c
	cc dpager.c -o dpager

hpager: hpager.c
	cc hpager.c -o hpager

test: test.c
	cc -static test.c -o test

clean:
	rm -f apager
	rm -f dpager
	rm -f hpager
	rm -f test
