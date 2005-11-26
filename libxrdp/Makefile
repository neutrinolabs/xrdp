
LIBXRDPOBJ = libxrdp.o xrdp_tcp.o xrdp_iso.o xrdp_mcs.o \
             xrdp_sec.o xrdp_rdp.o xrdp_orders.o \
             xrdp_bitmap_compress.o \
             os_calls.o ssl_calls.o file.o

CFLAGS = -Wall -O2 -I../common -fPIC
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
LIBS += -lcrypto
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

file.o:
	$(CC) $(C_OS_FLAGS) ../common/file.c

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c


ssl_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/ssl_calls.c
