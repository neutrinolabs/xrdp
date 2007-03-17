# sesman makefile
SESTESTOBJ = sestest.o tcp.o \
            os_calls.o d3des.o list.o file.o \
            libscp_v1c.o 

DEFINES = -DLIBSCP_CLIENT

CFLAGS = -Wall -O2 -I../../common -I../ -I/usr/include/nptl $(DEFINES)
#LDFLAGS = -L /usr/gnu/lib -I/usr/include/nptl -L/usr/lib/nptl -lpthread -ldl $(DEFINES) 
LDFLAGS = -L /usr/gnu/lib -ldl $(DEFINES) 
C_OS_FLAGS = $(CFLAGS) -c -g
CC = gcc

all: sestest 

sestest: $(SESTESTOBJ) 
	$(CC) $(LDFLAGS) -o sestest $(SESTESTOBJ) 

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c

d3des.o: ../../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../../common/d3des.c

list.o: ../../common/list.c
	$(CC) $(C_OS_FLAGS) ../../common/list.c

file.o: ../../common/file.c
	$(CC) $(C_OS_FLAGS) ../../common/file.c

tcp.o: ../tcp.c
	$(CC) $(C_OS_FLAGS) ../tcp.c

libscp_v1c.o: ../libscp_v1c.c
	$(CC) $(C_OS_FLAGS) ../libscp_v1c.c

clean:
	rm $(SESTESTOBJ) sestest

install:
	#install
