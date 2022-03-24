.PHONY: test libco

all: libco-test-64 libco-test-32

test: libco all
	@echo "==== TEST 64 bit ===="
	@LD_LIBRARY_PATH=.. ./libco-test-64
	@echo "==== TEST 32 bit ===="
	@LD_LIBRARY_PATH=.. ./libco-test-32

libco-test-64: main.c

libco:
	@cd .. && make -s

libco-test-64: main.c
	gcc -I.. -L.. -g -m64 main.c -o libco-test-64 -lco-64
#	objdump -d libco-test-64 > libco-test-64-obj.txt

libco-test-32: main.c
	gcc -I.. -L.. -g -m32 main.c -o libco-test-32 -lco-32
#	objdump -d libco-test-32 > libco-test-32-obj.txt

clean:
	rm -f libco-test-*
