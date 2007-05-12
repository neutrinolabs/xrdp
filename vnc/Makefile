# libvnc makefile

VNCOBJ = vnc.o os_calls.o d3des.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c -g
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: vnc

vnc: $(VNCOBJ)
	$(CC) $(LDFLAGS) -o libvnc.so $(VNCOBJ) $(LIBS)

clean:
	rm -f $(VNCOBJ) libvnc.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o: ../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../common/d3des.c

install:
	install libvnc.so $(DESTDIR)/libvnc.so

installdeb:
	install libvnc.so $(DESTDIRDEB)/usr/lib/xrdp/libvnc.so
