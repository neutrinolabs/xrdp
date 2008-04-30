# librdp makefile

RDPOBJ = ssl_calls.o os_calls.o rdp.o rdp_tcp.o rdp_iso.o rdp_mcs.o rdp_sec.o \
         rdp_orders.o rdp_bitmap.o rdp_rdp.o rdp_lic.o

LIBDIR = /usr/local/lib/xrdp

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
LIBS += -lcrypto
CC = gcc

all: librdp.so

librdp.so: $(RDPOBJ)
	$(CC) $(LDFLAGS) -o librdp.so $(RDPOBJ) $(LIBS)

clean:
	rm -f $(RDPOBJ) librdp.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

ssl_calls.o: ../common/ssl_calls.c
	$(CC) $(C_OS_FLAGS) ../common/ssl_calls.c

install:
	install librdp.so $(LIBDIR)/librdp.so
