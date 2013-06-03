

#PULSE_DIR=/home/jay/temp/pulseaudio-0.9.21
#PULSE_DIR=/home/jay/pulseaudio-0.9.22
#PULSE_DIR=/home/jay/pulseaudio-0.9.21
PULSE_DIR=/home/jay/pulseaudio-2.0

OBJS = module-xrdp-sink.o

CFLAGS = -Wall -O2 -I$(PULSE_DIR) -I$(PULSE_DIR)/src -DHAVE_CONFIG_H -fPIC

all: module-xrdp-sink.so

module-xrdp-sink.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o module-xrdp-sink.so $(OBJS)

clean:
	rm -f $(OBJS) module-xrdp-sink.so
