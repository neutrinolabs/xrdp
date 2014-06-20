
OBJS = rfxcodectest.o

CFLAGS = -g -O2 -Wall -fPIC -I../include

# this for linking to .so
#LDFLAGS = $(PROFIL) -L../src -Wl,-rpath=../src
# this if using .a
LDFLAGS = $(PROFIL)

# this for linking to .so
#LIBS = -lrfxencode
# this for using .a
LIBS = ../src/librfxencode.a

all: rfxcodectest

rfxcodectest: $(OBJS) Makefile
	$(CC) -o rfxcodectest $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) rfxcodectest
