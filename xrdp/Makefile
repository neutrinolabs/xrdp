# xrdp makefile
XRDPOBJ = xrdp.o xrdp_process.o xrdp_listen.o \
          xrdp_bitmap.o xrdp_wm.o xrdp_painter.o \
          xrdp_region.o xrdp_cache.o xrdp_font.o funcs.o \
          xrdp_login_wnd.o lang.o \
          list.o file.o os_calls.o thread_calls.o \
          xrdp_mm.o

DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

DEFINES = -DXRDP_CFG_FILE=\"$(CFGDIR)/xrdp.ini\" \
          -DXRDP_PID_FILE=\"$(PIDDIR)/xrdp.pid\"

CFLAGS = -Wall -O2 -I../common -I../libxrdp $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -L/usr/gnu/lib -L../libxrdp -Wl,-rpath,.
LIBS = -ldl -lpthread -lxrdp
CC = gcc

all: xrdp

xrdp: $(XRDPOBJ)
	$(CC) $(LDFLAGS) -o xrdp $(XRDPOBJ) $(LIBS)

clean:
	rm -f $(XRDPOBJ) xrdp

install:
	install ad256.bmp $(DESTDIR)/ad256.bmp
	install xrdp256.bmp $(DESTDIR)/xrdp256.bmp
	install cursor0.cur $(DESTDIR)/cursor0.cur
	install cursor1.cur $(DESTDIR)/cursor1.cur
	install Tahoma-10.fv1 $(DESTDIR)/Tahoma-10.fv1
	install xrdp.ini $(CFGDIR)/xrdp.ini
	install rsakeys.ini $(CFGDIR)/rsakeys.ini
	install xrdp $(DESTDIR)/xrdp

installdeb:
	install ad256.bmp $(DESTDIRDEB)/usr/lib/xrdp/ad256.bmp
	install xrdp256.bmp $(DESTDIRDEB)/usr/lib/xrdp/xrdp256.bmp
	install cursor0.cur $(DESTDIRDEB)/usr/lib/xrdp/cursor0.cur
	install cursor1.cur $(DESTDIRDEB)/usr/lib/xrdp/cursor1.cur
	install Tahoma-10.fv1 $(DESTDIRDEB)/usr/lib/xrdp/Tahoma-10.fv1
	install xrdp.ini $(DESTDIRDEB)/etc/xrdp/xrdp.ini
	install rsakeys.ini $(DESTDIRDEB)/etc/xrdp/rsakeys.ini
	install xrdp $(DESTDIRDEB)/usr/lib/xrdp/xrdp

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

thread_calls.o: ../common/thread_calls.c
	$(CC) $(C_OS_FLAGS) ../common/thread_calls.c

