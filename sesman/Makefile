
SESMANOBJ = sesman.o config.o tcp.o sig.o session.o env.o \
            ../common/os_calls.o \
            ../common/d3des.o \
            ../common/list.o \
            ../common/file.o

SESRUNOBJ = sesrun.o config.o tcp.o \
            ../common/os_calls.o \
            ../common/d3des.o \
            ../common/list.o \
            ../common/file.o

CFLAGS = -Wall -O2 -I../common
LDFLAGS = -L /usr/gnu/lib
CC = gcc

all: nopam tools

nopam: $(SESMANOBJ) verify_user.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user.o -ldl -lcrypt

pam: $(SESMANOBJ) verify_user_pam.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user_pam.o -ldl -lpam

pam_userpass: $(SESMANOBJ) verify_user_pam_userpass.o
	$(CC) $(LDFLAGS) -o sesman $(SESMANOBJ) verify_user_pam_userpass.o -ldl -lpam -lpam_userpass

tools:
	$(CC) $(LDFLAGS) -o sesrun $(SESRUNOBJ) -ldl

clean:
	rm -f $(SESMANOBJ) verify_user.o verify_user_pam.o verify_user_pam_userpass.o sesman sesrun.o sesrun
