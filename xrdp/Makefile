
XRDPOBJ = xrdp.o xrdp_process.o xrdp_listen.o \
          xrdp_bitmap.o xrdp_wm.o xrdp_painter.o \
          xrdp_region.o xrdp_cache.o xrdp_font.o funcs.o \
          xrdp_login_wnd.o xrdp_interface.o \
          ../common/list.o \
          ../common/file.o \
          ../common/os_calls.o \
          ../common/thread_calls.o

#CFLAGS = -Wall -O2 -I../common -I../libxrdp -DXRDP_DEBUG
CFLAGS = -Wall -O2 -I../common -I../libxrdp
LDFLAGS = -L/usr/gnu/lib -L../libxrdp -Wl,-rpath,.
LIBS = -ldl -lpthread -lxrdp
CC = gcc

all: xrdp

xrdp: $(XRDPOBJ)
	$(CC) $(LDFLAGS) -o xrdp $(XRDPOBJ) $(LIBS)
	strip xrdp

clean:
	rm -f $(XRDPOBJ) xrdp
