# sesman makefile
SESMANOBJ = sesman.o config.o tcp.o sig.o session.o env.o \
            os_calls.o d3des.o list.o file.o log.o access.o

SESRUNOBJ = sesrun.o config.o tcp.o \
            os_calls.o d3des.o list.o file.o log.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

DEFINES = -DSESMAN_CFG_FILE=\"$(CFGDIR)/sesman.ini\" \
          -DSESMAN_PID_FILE=\"$(PIDDIR)/sesman.pid\"

CFLAGS = -Wall -O2 -I../common $(DEFINES)
LDFLAGS = -L /usr/gnu/lib $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c
CC = gcc


all: pam tools

nopam: $(SESMANOBJ) verify_user.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user.o -ldl -lcrypt

pam: $(SESMANOBJ) verify_user_pam.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user_pam.o -ldl -lpam

pam_userpass: $(SESMANOBJ) verify_user_pam_userpass.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user_pam_userpass.o -ldl -lpam -lpam_userpass

kerberos: $(SESMANOBJ) verify_user_kerberos.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user_kerberos.o -ldl -lkrb5

tools: $(SESRUNOBJ)
	$(CC) $(LDFLAGS) -o sesrun $(SESRUNOBJ) -ldl

clean:
	rm -f $(SESMANOBJ) verify_user.o verify_user_pam.o verify_user_pam_userpass.o sesman sesrun.o sesrun

install:
	install sesman $(DESTDIR)/sesman
	install sesrun $(DESTDIR)/sesrun
	install startwm.sh $(DESTDIR)/startwm.sh
	install sesman.ini $(CFGDIR)/sesman.ini

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o: ../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../common/d3des.c

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c

log.o: ../common/log.c
	$(CC) $(C_OS_FLAGS) ../common/log.c
