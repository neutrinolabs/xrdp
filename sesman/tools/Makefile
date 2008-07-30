# sesman tools makefile
SESTESTOBJ = sestest.o \
            os_calls.o log.o
#	    d3des.o list.o file.o \
#            libscp_v1c.o tcp.o

SESRUNOBJ = sesrun.o config.o tcp.o \
            os_calls.o d3des.o list.o file.o log.o

SESADMINOBJ = sesadmin.o \
	      os_calls.o log.o

CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
LIBDIR = /usr/local/lib/xrdp
BINDIR = /usr/local/bin

DEFINES = -DLIBSCP_CLIENT

#CFLAGS = -Wall -O2 -I../../common -I../ -I/usr/include/nptl -I../libscp $(DEFINES)
CFLAGS = -Wall -g -I../../common -I../ -I/usr/include/nptl -I../libscp $(DEFINES)
LDFLAGS = -L/usr/gnu/lib -L/usr/lib/nptl -L../libscp -lpthread -ldl -lscp -Wl,-rpath,$(LIBDIR)
#LDFLAGS = -L /usr/gnu/lib -ldl $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c -g
CC = gcc

all: xrdp-sestest xrdp-sesrun 
#	xrdp-sesadmin

xrdp-sestest: $(SESTESTOBJ)
	$(CC) $(LDFLAGS) -o xrdp-sestest $(SESTESTOBJ)

xrdp-sesrun: $(SESRUNOBJ)
	$(CC) $(LDFLAGS) -o xrdp-sesrun $(SESRUNOBJ)

xrdp-sesadmin: $(SESADMINOBJ)
	$(CC) $(LDFLAGS) -o xrdp-sesadmin $(SESADMINOBJ)

clean:
	rm -f *.o xrdp-sestest xrdp-sesrun xrdp-sesadmin

install:
	install xrdp-sesrun $(BINDIR)/xrdp-sesrun
	install xrdp-sestest $(BINDIR)/xrdp-sestest

d3des.o: ../../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../../common/d3des.c

list.o: ../../common/list.c
	$(CC) $(C_OS_FLAGS) ../../common/list.c

file.o: ../../common/file.c
	$(CC) $(C_OS_FLAGS) ../../common/file.c

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c

log.o: ../../common/log.c
	$(CC) $(C_OS_FLAGS) ../../common/log.c

config.o: ../config.c
	$(CC) $(C_OS_FLAGS) ../config.c
