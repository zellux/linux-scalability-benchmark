CC=gcc
CFLAGS=-O2 -Wall -pthread

default: dirmaker dirmaker_fork

dirmaker: dirmaker.o
	$(CC) -o $@ $^ $(CFLAGS)

dirmaker_fork: dirmaker_fork.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf dirmaker dirmaker_fork
