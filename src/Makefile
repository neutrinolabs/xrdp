
OBJS = rfxencode.o rfxcompose.o rfxencode_tile.o rfxencode_dwt.o rfxencode_quantization.o \
rfxencode_differential.o rfxencode_rlgr3.o

CFLAGS = $(PROFIL) -g -O2 -Wall -fPIC -I../include

LDFLAGS =

LIBS =

all: librfxencode.so

librfxencode.so: $(OBJS) Makefile
	$(CC) -shared -o librfxencode.so $(LDFLAGS) $(OBJS) $(LIBS)
	$(AR) -rv librfxencode.a $(OBJS)

clean:
	rm -f $(OBJS) librfxencode.so
