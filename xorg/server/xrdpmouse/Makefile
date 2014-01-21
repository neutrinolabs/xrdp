
OBJS = rdpMouse.o

CFLAGS = -g -O2 -Wall -fPIC -I/usr/include/xorg -I/usr/include/pixman-1 \
-I../module -I../../../common

LDFLAGS =

LIBS =

all: xrdpmouse_drv.so

xrdpmouse_drv.so: $(OBJS) Makefile
	$(CC) -shared -o xrdpmouse_drv.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) xrdpmouse_drv.so
