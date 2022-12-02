#ifndef MEDIAPACKET_H
#define MEDIAPACKET_H

class MediaPacket
{
    public:
        MediaPacket();

        void *av_pkt;
        int   delay_in_us;
        int   seq;
};

#endif // MEDIAPACKET_H
