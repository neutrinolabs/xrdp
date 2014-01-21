
OBJS = rdpKeyboard.o

CFLAGS = -g -O2 -Wall -fPIC -I/usr/include/xorg -I/usr/include/pixman-1 \
-I../module -I../../../common

LDFLAGS =

LIBS =

all: xrdpkeyb_drv.so

xrdpkeyb_drv.so: $(OBJS) Makefile
	$(CC) -shared -o xrdpkeyb_drv.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) xrdpkeyb_drv.so
