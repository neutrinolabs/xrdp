# libxup makefile

XUPOBJ = os_calls.o xup.o

LIBDIR = /usr/local/lib/xrdp

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: libxup.so

libxup.so: $(XUPOBJ)
	$(CC) $(LDFLAGS) -o libxup.so $(XUPOBJ) $(LIBS)

clean:
	rm -f $(XUPOBJ) libxup.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

install:
	install libxup.so $(LIBDIR)/libxup.so
