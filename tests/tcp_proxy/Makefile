
CFLAGS = -O2 -Wall
LDFLAGS =
OBJS = main.o
LIBS = -ldl

all: tcp_proxy

tcp_proxy: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o tcp_proxy $(OBJS) $(LIBS)

.PHONY clean:
	rm -f $(OBJS) tcp_proxy
