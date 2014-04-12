
OBJS = rfxcodectest.o

CFLAGS = -g -O2 -Wall -fPIC -I../include

LDFLAGS = -L../src -Wl,-rpath=../src

LIBS = -lrfxencode

all: rfxcodectest

rfxcodectest: $(OBJS) Makefile
	$(CC) -o rfxcodectest $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) rfxcodectest
