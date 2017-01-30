
CFLAGS = -Werror -Wall

all: xdemo

xdemo: xdemo.c bmp_parser.c
	gcc $(CFLAGS) xdemo.c bmp_parser.c -o xdemo -lX11

.PHONY clean:
	rm -f *.o xdemo

