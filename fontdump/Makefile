
OBJS = fontdump.obj os_calls.obj

#CFLAGS = -O2 -I../common
CFLAGS = -O2 -I../common -DUNICODE -D_UNICODE
LDFLAGS = -W -efontdump.exe

all: fontdump1

fontdump1: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS)

clean:
	del $(OBJS) fontdump.exe *.tds

os_calls.obj: ../common/os_calls.c
	$(CC) $(CFLAGS) -c ../common/os_calls.c
