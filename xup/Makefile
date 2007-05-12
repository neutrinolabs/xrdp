# libxup makefile

XUPOBJ = os_calls.o xup.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: xup

xup: $(XUPOBJ)
	$(CC) $(LDFLAGS) -o libxup.so $(XUPOBJ) $(LIBS)

clean:
	rm -f $(XUPOBJ) libxup.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

install:
	install libxup.so $(DESTDIR)/libxup.so

installdeb:
	install libxup.so $(DESTDIRDEB)/usr/lib/xrdp/libxup.so
