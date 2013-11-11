
#include <unistd.h>
#include <sys/time.h>

#include "playvideo.h"
#include <QDebug>

PlayVideo::PlayVideo(QObject *parent,
                     QQueue<MediaPacket *> *videoQueue,
                     QMutex *sendMutex,
                     void *channel,
                     int stream_id, int fps) :
    QObject(parent)
{
    this->videoQueue = videoQueue;
    this->channel = channel;
    this->sendMutex = sendMutex;
    this->stream_id = stream_id;
    this->fps = fps;
}

/**
 ******************************************************************************/
static int
get_mstime(void)
{
    struct timeval tp;

    gettimeofday(&tp, 0);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

void PlayVideo::play()
{
    MediaPacket *pkt;
    int now_time;
    int sleep_time;
    int last_display_time;

    last_display_time = 0;
    while (1)
    {
        sendMutex->lock();
        if (videoQueue->isEmpty())
        {
            sendMutex->unlock();
            usleep(10 * 1000);
            continue;
        }
        pkt = videoQueue->dequeue();
        send_video_pkt(channel, stream_id, pkt->av_pkt);
        sendMutex->unlock();
        now_time = get_mstime();
        sleep_time = now_time - last_display_time;
        if (sleep_time > (1000 / fps))
        {
            sleep_time = (1000 / fps);
        }
        if (sleep_time > 0)
        {
            usleep(sleep_time * 1000);
        }
        last_display_time = now_time;
        delete pkt;
    }
}
