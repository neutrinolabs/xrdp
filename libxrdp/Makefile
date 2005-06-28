
LIBXRDPOBJ = libxrdp.o xrdp_tcp.o xrdp_iso.o xrdp_mcs.o \
             xrdp_sec.o xrdp_rdp.o xrdp_orders.o \
             xrdp_bitmap_compress.o \
             ../common/os_calls.o \
             ../common/ssl_calls.o

CFLAGS = -Wall -O2 -I../common -fPIC
LDFLAGS = -shared
LIBS = -ldl -lcrypto
CC = gcc

all: libxrdp

static: $(LIBXRDPOBJ)
	$(AR) rvu libxrdp.a $(LIBXRDPOBJ)
	ranlib libxrdp.a

libxrdp: $(LIBXRDPOBJ)
	$(CC) $(LDFLAGS) -o libxrdp.so $(LIBXRDPOBJ) $(LIBS)
	strip libxrdp.so

clean:
	rm -f $(LIBXRDPOBJ) libxrdp.a libxrdp.so

