
OBJS = libmem.o memtest.o

#CFLAGS = -O2 -Wall
CFLAGS = -g

all: memtest

memtest: $(OBJS)
	$(CC) -o memtest $(OBJS)

clean:
	rm -f $(OBJS) librdp.a
