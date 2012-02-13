
XRDP_INSTALL_BASE=/home/jay/xrdpinst

OBJS = main.o
CFLAGS = -O2 -Wall -I../../../common
LDFLAGS = -Wl,-rpath,$(XRDP_INSTALL_BASE)/lib/xrdp
LIBS = -L$(XRDP_INSTALL_BASE)/lib/xrdp -ldl -lcommon

all: tcp_proxy

tcp_proxy: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o tcp_proxy $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) tcp_proxy
