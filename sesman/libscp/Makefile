# sesman makefile
LIBSCPOBJ = libscp_vX.o libscp_v0.o \
	    libscp_v1s.o libscp_v1c.o \
	    libscp_init.o libscp_lock.o libscp_tcp.o \
	    os_calls.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp
LIBDIR = /usr/lib

DESTDIRDEB = /tmp

LIBSCPLNAME=libscp.so
LIBSCPSONAME=$(LIBSCPLNAME).1
LIBSCPFNAME=$(LIBSCPSONAME).0.0

DEFINES = -DSESMAN_CFG_FILE=\"$(CFGDIR)/sesman.ini\" \
          -DSESMAN_PID_FILE=\"$(PIDDIR)/sesman.pid\" \
          -DSESMAN_SESSVC_FILE=\"$(DESTDIR)/sessvc\" \
	  -DDEBUG

CFLAGS = -Wall -O2 -I../../common -I/usr/include/nptl -fPIC -g $(DEFINES)
LDFLAGS = -shared -Wl,-soname,$(LIBSCPSONAME) -L /usr/gnu/lib -I/usr/include/nptl -L/usr/lib/nptl -lpthread $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c
CC = gcc

all: $(LIBSCPOBJ)
	$(CC) $(LDFLAGS) -o $(LIBSCPFNAME) $(LIBSCPOBJ)
	ln -s $(LIBSCPFNAME) $(LIBSCPLNAME)

clean:
	rm -f $(LIBSCPOBJ) $(LIBSCPFNAME) $(LIBSCPLNAME)

install:
	install $(LIBSCPFNAME) $(LIBDIR)/$(LIBSCPFNAME)
	echo "cd $(LIBDIR) && ln -s $(LIBSCPSONAME) $(LIBSCPLNAME)" | /bin/sh
	ldconfig

installdeb:
	install $(LIBSCPFNAME) $(DESTDIRDEB)/$(LIBDIR)/$(LIBSCPFNAME)
	echo "cd $(DESTDIRDEB)/$(LIBDIR) && ln -s $(LIBSCPSONAME) $(LIBSCPLNAME)" | /bin/sh

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

log.o: ../common/log.c
	$(CC) $(C_OS_FLAGS) -DLOG_ENABLE_THREAD ../common/log.c

