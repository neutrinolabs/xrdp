
#include <unistd.h>

#include "demuxmedia.h"

DemuxMedia::DemuxMedia(QObject *parent, QQueue<MediaPacket *> *audioQueue,
                       QQueue<MediaPacket *> *videoQueue, void *channel, int stream_id) :
    QObject(parent)
{
    this->audioQueue = audioQueue;
    this->videoQueue = videoQueue;
    this->channel = channel;
    this->stream_id = stream_id;
    this->threadsStarted = false;
    this->vcrFlag = 0;

    playAudio = new PlayAudio(NULL, audioQueue, &sendMutex, channel, 101);
    playAudioThread = new QThread(this);
    connect(playAudioThread, SIGNAL(started()), playAudio, SLOT(play()));
    playAudio->moveToThread(playAudioThread);

    playVideo = new PlayVideo(NULL, videoQueue, &sendMutex, channel, 101);
    playVideoThread = new QThread(this);
    connect(playVideoThread, SIGNAL(started()), playVideo, SLOT(play()));
    playVideo->moveToThread(playVideoThread);
}

void DemuxMedia::setVcrOp(int op)
{
    vcrMutex.lock();
    vcrFlag = op;
    vcrMutex.unlock();

    if (playVideo)
        playVideo->setVcrOp(op);

    if (playAudio)
        playAudio->setVcrOp(op);
}

void DemuxMedia::startDemuxing()
{
    MediaPacket *mediaPkt;
    int          is_video_frame;
    int          rv;

    if ((audioQueue == NULL) || (videoQueue == NULL))
        return;

    while (1)
    {
        vcrMutex.lock();
        switch (vcrFlag)
        {
        case VCR_PLAY:
            vcrFlag = 0;
            vcrMutex.unlock();
            continue;
            break;

        case VCR_PAUSE:
            vcrMutex.unlock();
            usleep(1000 * 100);
            continue;
            break;

        case VCR_STOP:
            vcrMutex.unlock();
            usleep(1000 * 100);
            continue;
            break;

        default:
            vcrMutex.unlock();
            break;
        }

        if ((audioQueue->count() >= 20) || (videoQueue->count() >= 20))
        {
            if (!threadsStarted)
                startAudioVideoThreads();

            usleep(1000 * 20);
        }

        mediaPkt = new MediaPacket;
        rv = xrdpvr_get_frame(&mediaPkt->av_pkt,
                              &is_video_frame,
                              &mediaPkt->delay_in_us);
        if (rv < 0)
        {
            /* looks like we reached end of file */
            delete mediaPkt;
            playVideo->onMediaRestarted();
            usleep(1000 * 100);
            xrdpvr_seek_media(0, 0);
            continue;
        }

        if (is_video_frame)
            videoQueue->enqueue(mediaPkt);
        else
            audioQueue->enqueue(mediaPkt);

    } /* end while (1) */
}

PlayVideo * DemuxMedia::getPlayVideoInstance()
{
    return this->playVideo;
}

void DemuxMedia::startAudioVideoThreads()
{
    if (threadsStarted)
        return;

    playVideoThread->start();
    playAudioThread->start();
    threadsStarted = true;
}
