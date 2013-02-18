
CFLAGS = -Werror -Wall

all: test-randr

xdemo: trandr.c
	gcc $(CFLAGS) trandr.c -o test-randr -lX11

.PHONY clean:
	rm -f *.o test-randr

