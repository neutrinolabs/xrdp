
XRDPOBJ = xrdp.o os_calls.o xrdp_tcp.o xrdp_iso.o xrdp_mcs.o xrdp_sec.o \
          xrdp_rdp.o xrdp_process.o xrdp_listen.o xrdp_orders.o \
          xrdp_bitmap.o xrdp_wm.o xrdp_painter.o xrdp_list.o \
          xrdp_region.o xrdp_cache.o xrdp_font.o funcs.o
#CFLAGS = -Wall -O2 -DXRDP_DEBUG
CFLAGS = -Wall -O2
LDFLAGS = -L /usr/gnu/lib
LIBS = -lpthread -lcrypto
CC = gcc

all: xrdp

xrdp: $(XRDPOBJ)
	$(CC) $(LDFLAGS) -o xrdp $(XRDPOBJ) $(LIBS)

clean:
	rm -f *.o xrdp
