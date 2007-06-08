# libvnc makefile

KEYGENOBJ = keygen.o os_calls.o

DEFINES =

CFLAGS = -Wall -O2 -I../common $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS =
LIBS = -ldl -lcrypto
CC = gcc

all: keygen_

keygen_: $(KEYGENOBJ)
	$(CC) $(LDFLAGS) -o keygen $(KEYGENOBJ) $(LIBS)

clean:
	rm -f $(KEYGENOBJ) keygen

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c
