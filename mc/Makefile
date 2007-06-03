# libmc makefile
MCOBJ = os_calls.o mc.o

DEFINES =

CFLAGS = -Wall -O2 -I../common -fPIC $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -shared
LIBS = -ldl
CC = gcc

all: mc1

mc1: $(MCOBJ)
	$(CC) $(LDFLAGS) -o libmc.so $(MCOBJ) $(LIBS)

clean:
	rm -f $(MCOBJ) libmc.so

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c
