#
# build xrdp pulseaudio modules
#

# change this to your pulseaudio source directory
PULSE_DIR = /home/lk/pulseaudio-1.1
# change this if you're using non-default socket directory
SOCK_DIR  = /tmp/.xrdp
CFLAGS    = -Wall -O2 -I$(PULSE_DIR) -I$(PULSE_DIR)/src -DHAVE_CONFIG_H -fPIC -DXRDP_SOCKET_PATH=\"$(SOCK_DIR)\"

all: module-xrdp-sink.so module-xrdp-source.so

module-xrdp-sink.so: module-xrdp-sink.o
	$(CC) $(LDFLAGS) -shared -o module-xrdp-sink.so module-xrdp-sink.o

module-xrdp-source.so: module-xrdp-source.o
	$(CC) $(LDFLAGS) -shared -o module-xrdp-source.so module-xrdp-source.o

clean:
	rm -f module-xrdp-sink.o module-xrdp-sink.so module-xrdp-source.o module-xrdp-source.so
