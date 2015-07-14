
OBJS = xrdp_pcsc.o

CFLAGS = -Wall -O2 -fPIC

all: libpcsclite.so

libpcsclite.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o libpcsclite.so $(OBJS)

clean:
	rm -f $(OBJS) libpcsclite.so
