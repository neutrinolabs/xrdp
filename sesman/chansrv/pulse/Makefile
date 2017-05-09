#
# build xrdp pulseaudio modules
#

# change this to your pulseaudio source directory
PULSE_DIR = /tmp/pulseaudio-10.0
CFLAGS    = -Wall -O2 -I$(PULSE_DIR) -I$(PULSE_DIR)/src -DHAVE_CONFIG_H -fPIC

all: module-xrdp-sink.so module-xrdp-source.so

module-xrdp-sink.so: module-xrdp-sink.o
	$(CC) $(LDFLAGS) -shared -o module-xrdp-sink.so module-xrdp-sink.o

module-xrdp-source.so: module-xrdp-source.o
	$(CC) $(LDFLAGS) -shared -o module-xrdp-source.so module-xrdp-source.o

clean:
	rm -f module-xrdp-sink.o module-xrdp-sink.so module-xrdp-source.o module-xrdp-source.so
