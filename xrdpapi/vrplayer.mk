CFLAGS = -I../xrdpvr
LIBS = -L./.libs -L../xrdpvr/.libs -lxrdpapi -lxrdpvr -lavformat

vrplayer: vrplayer.o
	gcc $(CFLAGS) vrplayer.c -o vrplayer $(LIBS)
