
# := evaluates the expression just once
# = evaluates the expression each time it is used

CFLAGS := $(shell pkg-config --cflags gtk+-2.0)
CFLAGS += -O2 -Wall
#LDFLAGS = -Wl
LIBS := $(shell pkg-config --libs gtk+-2.0)
LIBS += -ldl -lgthread-2.0

OBJS = gtcp-proxy.o gtcp.o

all: gtcp-proxy

gtcp-proxy: $(OBJS)
	gcc -Wall $(CFLAGS) $(LDFLAGS) gtcp-proxy.c gtcp.o -o gtcp-proxy $(LIBS)

gtcp.o: gtcp.c
	gcc -c gtcp.c
clean::
	-rm gtcp-proxy.o gtcp.o gtcp-proxy
