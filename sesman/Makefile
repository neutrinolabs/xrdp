
SESMANOBJ = sesman.o os_calls.o d3des.o

CFLAGS = -Wall -O2 -I../common
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -L /usr/gnu/lib
LIBS = -lpam_userpass -lpam
PAMLIB =
CC = gcc

all: sesman

sesman: $(SESMANOBJ)
	$(CC) $(LDFLAGS) -o sesman $(PAMLIB) $(SESMANOBJ) $(LIBS)

clean:
	rm -f $(SESMANOBJ) sesman

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o:
	$(CC) $(C_OS_FLAGS) ../common/d3des.c
