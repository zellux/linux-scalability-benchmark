CC=gcc
CFLAGS=-O2 -Wall -pthread

sockbench: socketrun.o 
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf sockbench
