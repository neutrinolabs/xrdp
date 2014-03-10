
#include <unistd.h>

#include "demuxmedia.h"

DemuxMedia::DemuxMedia(QObject *parent, QQueue<MediaPacket *> *videoQueue,
                       void *channel, int stream_id) : QObject(parent)
{
    this->channel = channel;
    this->stream_id = stream_id;
    this->vcrFlag = 0;
    this->elapsedTime = 0;
    this->la_seekPos = -1;
    this->isStopped = 0;
    this->pausedTime = 0;
    this->videoQueue = videoQueue;

    playVideo = new PlayVideo(NULL, videoQueue, &sendMutex, channel, 101, 24);
    playVideoThread = new QThread(this);
    connect(playVideoThread, SIGNAL(started()), playVideo, SLOT(play()));
    playVideo->moveToThread(playVideoThread);

    playVideoThread->start();

}

void DemuxMedia::setVcrOp(int op)
{
    vcrMutex.lock();
    vcrFlag = op;
    vcrMutex.unlock();
    if (op == VCR_STOP)
    {
        clear();
    }
}

int DemuxMedia::clear()
{
    sendMutex.lock();
    videoQueue->clear();
    sendMutex.unlock();
    return 0;
}

void DemuxMedia::startDemuxing()
{
    MediaPacket *mediaPkt;
    int          is_video_frame;
    int          rv;

    while (1)
    {
        vcrMutex.lock();
        switch (vcrFlag)
        {
        case VCR_PLAY:
            vcrFlag = 0;
            vcrMutex.unlock();
            if (pausedTime)
            {
                elapsedTime = av_gettime() - pausedTime;
                pausedTime = 0;
            }
            isStopped = false;
            continue;
            break;

        case VCR_PAUSE:
            vcrMutex.unlock();
            if (!pausedTime)
            {
                /* save amount of video played so far */
                pausedTime = av_gettime() - elapsedTime;
            }
            usleep(1000 * 100);
            isStopped = false;
            continue;
            break;

        case VCR_STOP:
            vcrMutex.unlock();

            if (isStopped)
            {
                usleep(1000 * 100);
                continue;
            }
            elapsedTime = 0;
            pausedTime = 0;
            la_seekPos = -1;
            xrdpvr_seek_media(0, 0);
            isStopped = true;
            continue;
            break;

        default:
            vcrMutex.unlock();
            break;
        }

        mediaPkt = new MediaPacket;
        rv = xrdpvr_get_frame(&mediaPkt->av_pkt,
                              &is_video_frame,
                              &mediaPkt->delay_in_us);
        if (rv < 0)
        {
            /* looks like we reached end of file */
            delete mediaPkt;
            usleep(1000 * 100);
            xrdpvr_seek_media(0, 0);
            this->elapsedTime = 0;
            continue;
        }

        if (is_video_frame)
        {
            sendMutex.lock();
#if 1
            videoQueue->enqueue(mediaPkt);
#else
            send_video_pkt(channel, stream_id, mediaPkt->av_pkt);
            delete mediaPkt;
#endif
            sendMutex.unlock();
        }
        else
        {
            int frame;
            sendMutex.lock();
            send_audio_pkt(channel, stream_id, mediaPkt->av_pkt);
            sendMutex.unlock();
            xrdpvr_read_ack(channel, &frame);
            delete mediaPkt;
        }

        updateMediaPos();
        if (elapsedTime == 0)
        {
            elapsedTime = av_gettime();
        }

        /* time elapsed in 1/100th sec units since play started */
        emit onElapsedtime((av_gettime() - elapsedTime) / 10000);


    } /* end while (1) */
}

void DemuxMedia::onMediaSeek(int value)
{
    posMutex.lock();
    la_seekPos = value;
    posMutex.unlock();
}

void DemuxMedia::updateMediaPos()
{
    posMutex.lock();
    if (la_seekPos >= 0)
    {
        xrdpvr_seek_media(la_seekPos, 0);
        elapsedTime = av_gettime() - la_seekPos * 1000000;
        la_seekPos = -1;
    }
    posMutex.unlock();
}
