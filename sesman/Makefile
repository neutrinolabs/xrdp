# sesman makefile

SESMANOBJ = sesman.o config.o sig.o session.o env.o \
            os_calls.o d3des.o list.o file.o log.o access.o \
            scp.o scp_v0.o scp_v1.o scp_v1_mng.o thread.o lock.o

SESSVCOBJ = sessvc.o os_calls.o log.o

CFGDIR = /etc/xrdp
PIDDIR = /var/run
LIBDIR = /usr/local/lib/xrdp
SBINDIR = /usr/local/sbin

DEFINES = -DSESMAN_CFG_FILE=\"$(CFGDIR)/sesman.ini\" \
          -DSESMAN_PID_FILE=\"$(PIDDIR)/xrdp-sesman.pid\" -DDEBUG

CFLAGS = -Wall -O2 -I../common -I/usr/include/nptl -I./libscp $(DEFINES)
LDFLAGS = -L/usr/gnu/lib -L/usr/lib/nptl -L./libscp -Wl,-rpath,$(LIBDIR) -lpthread -ldl -lscp
C_OS_FLAGS = $(CFLAGS) -c
CC = gcc

all: libscp_ pam_base xrdp-sessvc tools_

nopam: libscp_ no_pam_base xrdp-sessvc tools_

pamuserpass: libscp_ pam_userpass_base xrdp-sessvc tools

kerberos: libscp_ kerberos_base xrdp-sessvc tools

pam_base: $(SESMANOBJ) verify_user_pam.o
	$(CC) $(LDFLAGS) -o xrdp-sesman $(SESMANOBJ) verify_user_pam.o -lpam

no_pam_base: $(SESMANOBJ) verify_user.o
	make -C libscp
	$(CC) $(LDFLAGS) -o xrdp-sesman $(SESMANOBJ) verify_user.o -lcrypt

pam_userpass_base: $(SESMANOBJ) verify_user_pam_userpass.o
	$(CC) $(LDFLAGS) -o xrdp-sesman $(SESMANOBJ) verify_user_pam_userpass.o -lpam -lpam_userpass

kerberos_base: $(SESMANOBJ) verify_user_kerberos.o
	$(CC) $(LDFLAGS) -o xrdp-sesman $(SESMANOBJ) verify_user_kerberos.o -lkrb5

xrdp-sessvc: $(SESSVCOBJ)
	$(CC) $(LDFLAGS) -o xrdp-sessvc $(SESSVCOBJ)

tools_:
	make -C tools

libscp_:
	make -C libscp

clean:
	rm -f $(SESMANOBJ) verify_user.o verify_user_pam.o verify_user_pam_userpass.o verify_user_kerberos.o xrdp-sesman sessvc.o xrdp-sessvc
	make -C tools clean
	make -C libscp clean

install:
	install xrdp-sesman $(SBINDIR)/xrdp-sesman
	install startwm.sh $(CFGDIR)/startwm.sh
	install sesman.ini $(CFGDIR)/sesman.ini
	install xrdp-sessvc $(SBINDIR)/xrdp-sessvc
	make -C tools install
	make -C libscp install

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o: ../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../common/d3des.c

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c

log.o: ../common/log.c
	$(CC) $(C_OS_FLAGS) -DLOG_ENABLE_THREAD ../common/log.c

