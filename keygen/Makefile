# libvnc makefile

KEYGENOBJ = keygen.o os_calls.o ssl_calls.o list.o file.o

DEFINES =

BINDIR = /usr/local/bin

CFLAGS = -Wall -O2 -I../common $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS =
LIBS = -ldl -lcrypto
CC = gcc

all: xrdp-keygen

xrdp-keygen: $(KEYGENOBJ)
	$(CC) $(LDFLAGS) -o xrdp-keygen $(KEYGENOBJ) $(LIBS)

clean:
	rm -f $(KEYGENOBJ) xrdp-keygen

install:
	install xrdp-keygen $(BINDIR)/xrdp-keygen

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

ssl_calls.o: ../common/ssl_calls.c
	$(CC) $(C_OS_FLAGS) ../common/ssl_calls.c

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c
