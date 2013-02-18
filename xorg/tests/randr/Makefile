
CFLAGS = -O2 -Wall

all: test-randr

test-randr: trandr.c
	gcc $(CFLAGS) trandr.c -o test-randr -lX11 -lXrandr

.PHONY clean:
	rm -f *.o test-randr

