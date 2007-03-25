# libvnc makefile
LIBSCPOBJ = libscp_v0.o tcp.o

VNCOBJ = vnc.o os_calls.o d3des.o $(LIBSCPOBJ)

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

libscp_v0.o:
	$(CC) $(C_OS_FLAGS) ../sesman/libscp_v0.c

tcp.o:
	$(CC) $(C_OS_FLAGS) -DLIBSCP_CLIENT ../sesman/tcp.c