CC=gcc
CFLAGS=-O2 -Wall -pthread

default: mmapbench

forkbench: mmapbench.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf mmapbench
