# xrdp makefile
XRDPOBJ = xrdp.o xrdp_process.o xrdp_listen.o \
          xrdp_bitmap.o xrdp_wm.o xrdp_painter.o \
          xrdp_region.o xrdp_cache.o xrdp_font.o funcs.o \
          xrdp_login_wnd.o lang.o \
          list.o file.o os_calls.o thread_calls.o \
          xrdp_mm.o

CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
LIBDIR = /usr/local/lib/xrdp
BINDIR = /usr/local/bin
SBINDIR = /usr/local/sbin
SHAREDIR = /usr/local/share/xrdp

DEFINES = -DXRDP_CFG_FILE=\"$(CFGDIR)/xrdp.ini\" \
          -DXRDP_PID_FILE=\"$(PIDDIR)/xrdp.pid\"

CFLAGS = -Wall -O2 -I../common -I../libxrdp $(DEFINES)
#CFLAGS += -DXRDP_DEBUG
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -L/usr/gnu/lib -L../libxrdp -Wl,-rpath,$(LIBDIR)
LIBS = -ldl -lpthread -lxrdp
CC = gcc

all: xrdp

xrdp: $(XRDPOBJ)
	$(CC) $(LDFLAGS) -o xrdp $(XRDPOBJ) $(LIBS)

clean:
	rm -f $(XRDPOBJ) xrdp

install:
	install ad256.bmp $(SHAREDIR)/ad256.bmp
	install xrdp256.bmp $(SHAREDIR)/xrdp256.bmp
	install cursor0.cur $(SHAREDIR)/cursor0.cur
	install cursor1.cur $(SHAREDIR)/cursor1.cur
	install sans-10.fv1 $(SHAREDIR)/sans-10.fv1
	install xrdp.ini $(CFGDIR)/xrdp.ini
	install rsakeys.ini $(CFGDIR)/rsakeys.ini
	install xrdp $(SBINDIR)/xrdp

list.o: ../common/list.c
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o: ../common/file.c
	$(CC) $(C_OS_FLAGS) ../common/file.c

os_calls.o: ../common/os_calls.c
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

thread_calls.o: ../common/thread_calls.c
	$(CC) $(C_OS_FLAGS) ../common/thread_calls.c
