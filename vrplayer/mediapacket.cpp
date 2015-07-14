#include "mediapacket.h"

MediaPacket::MediaPacket()
{
    av_pkt = 0;
    delay_in_us = 0;
    seq = 0;
}
