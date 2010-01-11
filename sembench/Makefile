CC=gcc
CFLAGS=-O2 -Wall -pthread

default: pingpong

pingpong: pingpong.o config.h
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf pingpong
