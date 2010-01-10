CC=gcc
CFLAGS=-O2 -Wall -pthread

default: sembench pingpong

sembench: sembench.o config.h
	$(CC) -o $@ $^ $(CFLAGS)

pingpong: pingpong.o config.h
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf sembench pingpong
