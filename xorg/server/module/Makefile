
OBJS = rdpMain.o

CFLAGS = -O2 -Wall -I/usr/include/xorg -I/usr/include/pixman-1

LDFLAGS =

LIBS =

all: xrdpdev_drv.so

xrdpdev_drv.so: $(OBJS) Makefile
	$(CC) -shared -o libxorgxrdp.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) libxorgxrdp.so
