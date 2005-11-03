
RDPOBJ = ssl_calls.o os_calls.o rdp.o rdp_tcp.o rdp_iso.o rdp_mcs.o rdp_sec.o \
         rdp_orders.o rdp_bitmap.o rdp_rdp.o rdp_lic.o
CFLAGS = -Wall -O2 -I../common -fPIC
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl -lcrypto
CC = gcc

all: rdp

rdp: $(RDPOBJ)
	$(CC) $(LDFLAGS) -o librdp.so $(RDPOBJ) $(LIBS)

clean:
	rm -f $(RDPOBJ) librdp.so

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

ssl_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/ssl_calls.c
