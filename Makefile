test: test.c l3.c l3.S l3.h
	gcc -Wall -o test -g -O3 test.c l3.c l3.S l3.h
	./test
	python l3_dump.py /tmp/l3_test test
	python l3_dump.py /tmp/l3_small_test test
