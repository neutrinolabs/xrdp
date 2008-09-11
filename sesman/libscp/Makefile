# libscp makefile
LIBSCPOBJ = libscp_vX.o libscp_v0.o \
	    libscp_v1s.o libscp_v1c.o \
	    libscp_init.o libscp_lock.o libscp_tcp.o \
	    libscp_session.o libscp_connection.o \
	    os_calls.o \
	    libscp_v1s_mng.o libscp_v1c_mng.o

CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
LIBDIR = /usr/local/lib/xrdp

LIBSCPLNAME = libscp.so
LIBSCPSONAME = $(LIBSCPLNAME).1
LIBSCPFNAME = $(LIBSCPSONAME).0.0

DEFINES = -DSESMAN_CFG_FILE=\"$(CFGDIR)/sesman.ini\" \
	  -DSESMAN_PID_FILE=\"$(PIDDIR)/xrdp-sesman.pid\" \
	  -DSESMAN_SESSVC_FILE=\"xrdp-sessvc\"

#CFLAGS = -Wall -O2 -I../../common -I/usr/include/nptl -fPIC $(DEFINES)
CFLAGS = -Wall -g -I../../common -I/usr/include/nptl -fPIC $(DEFINES)
LDFLAGS = -shared -L/usr/gnu/lib -L/usr/lib/nptl -lpthread
C_OS_FLAGS = $(CFLAGS) -c
CC = gcc

all: libscp.so

libscp.so: $(LIBSCPOBJ)
	$(CC) $(LDFLAGS) -o $(LIBSCPFNAME) $(LIBSCPOBJ)
	ln -f -s $(LIBSCPFNAME) $(LIBSCPLNAME)

clean:
	rm -f $(LIBSCPOBJ) $(LIBSCPFNAME) $(LIBSCPLNAME)

install:
	install -D $(LIBSCPFNAME) $(LIBDIR)/$(LIBSCPFNAME)
	ln -f -s $(LIBSCPFNAME) $(LIBDIR)/$(LIBSCPLNAME)

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c
