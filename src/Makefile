
OBJS = rfxencode.o rfxcompose.o

CFLAGS = -g -O2 -Wall -fPIC -I../include

LDFLAGS =

LIBS =

all: librfxencode.so

librfxencode.so: $(OBJS) Makefile
	$(CC) -shared -o librfxencode.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) librfxencode.so
