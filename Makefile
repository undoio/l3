# #############################################################################
# Simple Makefile to build and test Lightweight Logging Library (l3) sources
# #############################################################################
test: test.c l3.c l3.S l3.h
	pylint l3_dump.py
	gcc -Wall -o test -g -O3 test.c l3.c l3.S
	./test
	@echo
	./l3_dump.py
	@echo
	python3 l3_dump.py /tmp/l3_test ./test
	@echo
	python3 l3_dump.py /tmp/l3_small_test ./test
