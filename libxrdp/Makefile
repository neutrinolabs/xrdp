# libxrdp makefile
LIBXRDPOBJ = libxrdp.o xrdp_tcp.o xrdp_iso.o xrdp_mcs.o \
             xrdp_sec.o xrdp_rdp.o xrdp_orders.o \
             xrdp_bitmap_compress.o xrdp_channel.o \
             os_calls.o ssl_calls.o file.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

DEFINES = -DXRDP_CFG_FILE=\"$(CFGDIR)/xrdp.ini\" \
          -DXRDP_KEY_FILE=\"$(CFGDIR)/rsakeys.ini\"

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
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

clean:
	rm -f $(LIBXRDPOBJ) libxrdp.a libxrdp.so

install:
	install libxrdp.so $(DESTDIR)/libxrdp.so

installdeb:
	install libxrdp.so $(DESTDIRDEB)/usr/lib/xrdp/libxrdp.so

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c


ssl_calls.o: ../common/ssl_calls.c
	$(CC) $(C_OS_FLAGS) ../common/ssl_calls.c
