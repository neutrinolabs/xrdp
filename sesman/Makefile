
# uncomment the next line to use pam_userpass
# in verify_user.c
#USE_PAM = ""

SESMANOBJ = sesman.o verify_user.o ../common/os_calls.o ../common/d3des.o

ifdef USE_PAM
CFLAGS = -Wall -O2 -I../common -DUSE_PAM
else
CFLAGS = -Wall -O2 -I../common
endif
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -L /usr/gnu/lib
ifdef USE_PAM
LIBS = -lpam -lpam_userpass
else
LIBS = -ldl -lcrypt
endif
PAMLIB =
CC = gcc

all: sesman

sesman: $(SESMANOBJ)
	$(CC) $(LDFLAGS) -o sesman $(PAMLIB) $(SESMANOBJ) $(LIBS)

clean:
	rm -f $(SESMANOBJ) sesman
