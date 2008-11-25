
SESSVCOBJ = sessvc.o os_calls.o log.o

CFGDIR = /etc/xrdp
PIDDIR = /var/run
LIBDIR = /usr/local/lib/xrdp
SBINDIR = /usr/local/sbin

CFLAGS = -Wall -O2 -I../../common
LDFLAGS = -lpthread -ldl
C_OS_FLAGS = $(CFLAGS) -c
CC = gcc

all: xrdp-sessvc

xrdp-sessvc: $(SESSVCOBJ)
	$(CC) $(LDFLAGS) -o xrdp-sessvc $(SESSVCOBJ)

clean:
	rm -f $(SESSVCOBJ) xrdp-sessvc

install:
	install xrdp-sessvc $(SBINDIR)/xrdp-sessvc

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c

log.o: ../../common/log.c
	$(CC) $(C_OS_FLAGS) -DLOG_ENABLE_THREAD ../../common/log.c
