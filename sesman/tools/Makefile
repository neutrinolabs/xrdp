# sesman tools makefile
SESTESTOBJ = sestest.o tcp.o \
            os_calls.o d3des.o list.o file.o \
            libscp_v1c.o

SESRUNOBJ = sesrun.o config.o tcp.o \
            os_calls.o d3des.o list.o file.o log.o

DEFINES = -DLIBSCP_CLIENT

CFLAGS = -Wall -O2 -I../../common -I../ -I/usr/include/nptl $(DEFINES)
LDFLAGS = -L /usr/gnu/lib -I/usr/include/nptl -L/usr/lib/nptl -lpthread -ldl $(DEFINES)
#LDFLAGS = -L /usr/gnu/lib -ldl $(DEFINES)
C_OS_FLAGS = $(CFLAGS) -c -g
CC = gcc

all: stest srun

stest: $(SESTESTOBJ)
	$(CC) $(LDFLAGS) -o sestest $(SESTESTOBJ)

srun: $(SESRUNOBJ)
	$(CC) $(LDFLAGS) -o sesrun $(SESRUNOBJ)

os_calls.o: ../../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../../common/os_calls.c

d3des.o: ../../common/d3des.c
	$(CC) $(C_OS_FLAGS) ../../common/d3des.c

list.o: ../../common/list.c
	$(CC) $(C_OS_FLAGS) ../../common/list.c

file.o: ../../common/file.c
	$(CC) $(C_OS_FLAGS) ../../common/file.c

log.o: ../../common/file.c
	$(CC) $(C_OS_FLAGS) ../../common/log.c

tcp.o: ../tcp.c
	$(CC) $(C_OS_FLAGS) ../tcp.c

config.o: ../config.c
	$(CC) $(C_OS_FLAGS) ../config.c

libscp_v1c.o: ../libscp_v1c.c
	$(CC) $(C_OS_FLAGS) ../libscp_v1c.c

clean:
	rm -f *.o sestest sesrun

install:
	install sesrun $(DESTDIR)/sesrun
	install sestest $(DESTDIR)/sestest

installdeb:
	install sesrun $(DESTDIRDEB)/usr/lib/xrdp/sesrun
	install sestest $(DESTDIRDEB)/usr/lib/xrdp/sestest
