
OBJS = xrdpdev.o

CFLAGS = -g -O2 -Wall -fPIC -I/usr/include/xorg -I/usr/include/pixman-1 -I../module

LDFLAGS =

LIBS =

all: xrdpdev_drv.so

xrdpdev_drv.so: $(OBJS) Makefile
	$(CC) -shared -o xrdpdev_drv.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) xrdpdev_drv.so
