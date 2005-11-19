# xrdp makefile
XRDPOBJ = xrdp.o xrdp_process.o xrdp_listen.o \
          xrdp_bitmap.o xrdp_wm.o xrdp_painter.o \
          xrdp_region.o xrdp_cache.o xrdp_font.o funcs.o \
          xrdp_login_wnd.o xrdp_interface.o \
          list.o file.o os_calls.o thread_calls.o

#CFLAGS = -Wall -O2 -I../common -I../libxrdp -DXRDP_DEBUG
CFLAGS = -Wall -O2 -I../common -I../libxrdp
C_OS_FLAGS = $(CFLAGS) -c
LDFLAGS = -L/usr/gnu/lib -L../libxrdp -Wl,-rpath,.
LIBS = -ldl -lpthread -lxrdp
CC = gcc

all: xrdp

xrdp: $(XRDPOBJ)
	$(CC) $(LDFLAGS) -o xrdp $(XRDPOBJ) $(LIBS)
	strip xrdp

clean:
	rm -f $(XRDPOBJ) xrdp

list.o:
	$(CC) $(C_OS_FLAGS) ../common/list.c

file.o:
	$(CC) $(C_OS_FLAGS) ../common/file.c

os_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/os_calls.c

thread_calls.o:
	$(CC) $(C_OS_FLAGS) ../common/thread_calls.c

