apager: apager.c
	cc apager.c -o apager

test:
	cc -static test.c -o test

clean:
	rm -f apager
	rm -f test
