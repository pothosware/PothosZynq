########################################################################
## Simple Makefile for cross compiling DMA loopback test application
########################################################################
CC=$(CROSS_COMPILE)gcc

INCLUDES=-I./ -I../kernel/

CFLAGS=-std=c99 -O3 $(INCLUDES)

LDFLAGS=-static

OBJ = loopback_test.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

loopback_test.exe: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf *.o *.exe
