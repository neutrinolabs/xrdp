# libvnc makefile

VNCOBJ = vnc.o os_calls.o d3des.o

LIBDIR = /usr/local/lib/xrdp

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c -g
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: libvnc.so

libvnc.so: $(VNCOBJ)
	$(CC) $(LDFLAGS) -o libvnc.so $(VNCOBJ) $(LIBS)

clean:
	rm -f $(VNCOBJ) libvnc.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o: ../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../common/d3des.c

install:
	install libvnc.so $(LIBDIR)/libvnc.so
