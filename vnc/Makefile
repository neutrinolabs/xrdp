
VNCOBJ = ../common/os_calls.o vnc.o d3des.o vncauth.o
CFLAGS = -Wall -O2 -I../common
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: vnc

vnc: $(VNCOBJ)
	$(CC) $(LDFLAGS) -o libvnc.so $(VNCOBJ) $(LIBS)

clean:
	rm -f $(VNCOBJ) libvnc.so
