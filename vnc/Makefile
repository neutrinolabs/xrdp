
VNCOBJ = vnc.o os_calls.o d3des.o
CFLAGS = -Wall -O2 -I../common
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: vnc

vnc: $(VNCOBJ)
	$(CC) $(LDFLAGS) -o libvnc.so $(VNCOBJ) $(LIBS)

clean:
	rm -f $(VNCOBJ) libvnc.so

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

d3des.o:
	$(CC) $(C_OS_FLAGS) ../common/d3des.c
