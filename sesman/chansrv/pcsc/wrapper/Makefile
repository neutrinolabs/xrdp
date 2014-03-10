
CC=bcc32.exe

CFLAGS=-O2

OBJS=winscard.obj

winscard.dll: $(OBJS)
	$(CC) -ewinscard.dll -tWD $(OBJS)

clean:
	-del winscard.dll
	-del *.obj
	-del *.tds
