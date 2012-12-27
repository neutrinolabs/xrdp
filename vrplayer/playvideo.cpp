#include "playvideo.h"
#include <QDebug>

PlayVideo::PlayVideo(QObject *parent,
                     QQueue<MediaPacket *> *videoQueue,
                     QMutex *sendMutex,
                     void *channel,
                     int stream_id) :
    QObject(parent)
{
    this->videoQueue = videoQueue;
    this->channel = channel;
    this->sendMutex = sendMutex;
    this->stream_id = stream_id;
    elapsedTime = 0;
    pausedTime = 0;
    la_seekPos = -1;
    vcrFlag = 0;
    isStopped = false;
}

void PlayVideo::play()
{
    MediaPacket *pkt;

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
            clearVideoQ();
            elapsedTime = 0;
            pausedTime = 0;
            la_seekPos = -1;
            xrdpvr_seek_media(0, 0);
            isStopped = true;
            continue;
            break;

        default:
            vcrMutex.unlock();
            goto label1;
            break;
        }

label1:

        if (videoQueue->isEmpty())
        {
            qDebug() << "PlayVideo::play: GOT EMPTY";
            usleep(1000 * 100);
            continue;
        }

        pkt = videoQueue->dequeue();
        sendMutex->lock();
        send_video_pkt(channel, stream_id, pkt->av_pkt);
        sendMutex->unlock();
        delete pkt;
        usleep(pkt->delay_in_us);

        updateMediaPos();

        if (elapsedTime == 0)
            elapsedTime = av_gettime();

        /* time elapsed in 1/100th sec units since play started */
        emit onElapsedtime((av_gettime() - elapsedTime) / 10000);
    }
}

void PlayVideo::onMediaSeek(int value)
{
    posMutex.lock();
    la_seekPos = value;
    posMutex.unlock();
}

void PlayVideo::updateMediaPos()
{
#if 0
    if (elapsedTime == 0)
        elapsedTime = av_gettime();

    /* time elapsed in 1/100th sec units since play started */
    emit onElapsedtime((av_gettime() - elapsedTime) / 10000);
#endif

    posMutex.lock();
    if (la_seekPos >= 0)
    {
        qDebug() << "seeking to" << la_seekPos;
        xrdpvr_seek_media(la_seekPos, 0);
        elapsedTime = av_gettime() - la_seekPos * 1000000;
        la_seekPos = -1;
    }
    posMutex.unlock();
}

void PlayVideo::setVcrOp(int op)
{
    vcrMutex.lock();
    this->vcrFlag = op;
    vcrMutex.unlock();
}

void PlayVideo::clearVideoQ()
{
    MediaPacket *pkt;

    while (!videoQueue->isEmpty())
    {
        pkt = videoQueue->dequeue();
        av_free_packet((AVPacket *) pkt->av_pkt);
        delete pkt;
    }
}

#if 0
void DecoderThread::updateSlider()
{
    if (elapsedTime == 0)
        elapsedTime = av_gettime();

    /* time elapsed in 1/100th sec units since play started */
    emit onElapsedtime((av_gettime() - elapsedTime) / 10000);

    mutex.lock();
    if (la_seekPos >= 0)
    {
        qDebug() << "seeking to" << la_seekPos;
        //audioTimer->stop();
        //videoTimer->stop();
        xrdpvr_seek_media(la_seekPos, 0);
        elapsedTime = av_gettime() - la_seekPos * 1000000;
        //audioTimer->start(10);
        //videoTimer->start(10);
        la_seekPos = -1;
    }
    mutex.unlock();
}
#endif
