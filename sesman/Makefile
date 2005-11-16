
SESMANOBJ = sesman.o config.o tcp.o sig.o session.o env.o \
            os_calls.o d3des.o list.o file.o log.o

SESRUNOBJ = sesrun.o config.o tcp.o \
            os_calls.o d3des.o list.o file.o log.o

CFLAGS = -Wall -O2 -I../common
LDFLAGS = -L /usr/gnu/lib
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
	rm -f $(SESMANOBJ) verify_user.o verify_user_pam.o verify_user_pam_userpass.o verify_user_kerberos.o sesman sesrun.o sesrun

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o:
	$(CC) $(C_OS_FLAGS) ../common/d3des.c

list.o:
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o:
	$(CC) $(C_OS_FLAGS) ../common/file.c

log.o:
	$(CC) $(C_OS_FLAGS) ../common/log.c
