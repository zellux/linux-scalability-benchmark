CC=gcc
CFLAGS=-O2 -Wall -pthread

socketrun: socketrun.o helper.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf socketrun