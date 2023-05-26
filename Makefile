obj-m += morse.o
.PHONY: all build load unload clean

all: test build
debug: unload build load

build:
	make -C /lib/modules/$(shell uname -r)/build modules M=$(shell pwd)

load:
	-sudo insmod morse.ko

unload:
	-sudo rmmod morse

test: test.c
	gcc test.c -o test -Wall -Wextra -Wpedantic

clean:
	rm -f *~ test
	-rm -f test
