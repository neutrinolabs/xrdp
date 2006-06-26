# makefile

OBJS = tcp.obj iso.obj mcs.obj secure.obj rdp.obj rdp5.obj orders.obj \
bitmap.obj cache.obj mppc.obj pstcache.obj channels.obj licence.obj \
ssl_calls.obj

UIOBJS = uimain.obj bsops.obj win32.obj

#-w-aus -w-rch
CFLAGS = -O2 -w-par -I.. -Ic:\borland\bcc55\include
LDFLAGS = -W -Lc:\borland\bcc55\lib

all: rd

rd: $(OBJS) $(UIOBJS)
	$(CC) -ewinrdesktop.exe $(LDFLAGS) $(OBJS) $(UIOBJS)

clean:
	del /q $(OBJS) $(UIOBJS) winrdesktop.exe *.tds
