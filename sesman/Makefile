
SESMANOBJ = sesman.o ../common/os_calls.o ../common/d3des.o

CFLAGS = -Wall -O2 -I../common
LDFLAGS = -L /usr/gnu/lib
LIBS = -lpam_userpass -lcrypto -lpthread
PAMLIB = /lib/libpam.so.0
CC = gcc

all: sesman

sesman: $(SESMANOBJ)
	$(CC) $(LDFLAGS) -o sesman $(PAMLIB) $(SESMANOBJ) $(LIBS)

clean:
	rm -f $(SESMANOBJ) sesman
