

// xrdp_chan_test.cpp : Basic test for virtual channel use.

// These headers are required for the windows terminal services calls.
#include "xrdpapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DSIZE (1024 * 4)

int main()
{

	// Initialize the data for send/receive
	void* hFile;
	char* data;
	char* data1;
	data = (char*)malloc(DSIZE);
	data1 = (char*)malloc(DSIZE);
	int ret;
	void* vcFileHandlePtr = NULL;
	memset(data, 0xca, DSIZE);
	memset(data1, 0, DSIZE);
	unsigned int written = 0;

	// Open the skel channel in current session

	//void* channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "skel", 0);
	void* channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "TSMF", WTS_CHANNEL_OPTION_DYNAMIC);
	ret = WTSVirtualChannelQuery(channel, WTSVirtualFileHandle, vcFileHandlePtr, &written);

	// Write the data to the channel
	ret = WTSVirtualChannelWrite(channel, data, DSIZE, &written);
	if (!ret)
	{

		long err = errno;
		printf("error 1 0x%8.8x\n", err);
		usleep(100000);
		return 1;
	}
	else
	{
		printf("Sent bytes!\n");
	}
	if (written != DSIZE)
	{
		long err = errno;
		printf("error 2 0x%8.8x\n", err);
		usleep(100000);
		return 1;
	}
	else
	{
		printf("Read bytes!\n");
	}
	ret = WTSVirtualChannelRead(channel, 100, data1, DSIZE, &written);
	if (!ret)
	{
		long err = errno;
		printf("error 3 0x%8.8x\n", err);
		usleep(100000);
		return 1;
	}
	if (written != DSIZE)
	{
		long err = errno;
		printf("error 4 0x%8.8x\n", err);
		usleep(100000);
		return 1;
	}
	else
	{
		printf("Read bytes!\n");
	}
	ret = WTSVirtualChannelClose(channel);
	if (memcmp(data, data1, DSIZE) == 0)
	{
	}
	else
	{
		printf("error data no match\n");
		return 1;
	}

	printf("Done!\n");

	usleep(100000);
	return 0;
}
