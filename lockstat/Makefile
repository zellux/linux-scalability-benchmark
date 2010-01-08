# Makefile for lockstat

# LINUX_INC_ROOT is the root of the include directory of the Linux tree
# for which you want to compile.  If you want to compile for a Linux version
# other than the one currently installed on your machine, define it in
# your environment or on the make command line.
ifndef LINUX_INC_ROOT
LINUX_INC_ROOT = ../../linux/linux/include
endif

PROGS	= lockstat
CC	= gcc
CFLAGS	= -O2 -fomit-frame-pointer -I$(LINUX_INC_ROOT)

default all: $(PROGS)

OBJS = lockstat.o symbols.o getsetdata.o

.o:
	$(CC) $(CFLAGS) -c $*.c

lockstat: $(OBJS)
	$(CC) $(LDFLAGS) -o lockstat $(OBJS) $(LDOPT)

install: lockstat
	install -m 0755 lockstat $(INSTALLROOT)/usr/sbin

clean:
	$(RM) $(PROGS) *.o
