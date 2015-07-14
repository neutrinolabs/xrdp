
#include <unistd.h>

#include "playaudio.h"
#include <QDebug>

PlayAudio::PlayAudio(QObject *parent,
                     QQueue<MediaPacket *> *audioQueue,
                     QMutex *sendMutex,
                     void *channel,
                     int stream_id) :
    QObject(parent)
{
    this->audioQueue = audioQueue;
    this->sendMutex = sendMutex;
    this->channel = channel;
    this->stream_id = stream_id;
    this->vcrFlag = 0;
}

void PlayAudio::play()
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
            continue;
            break;

        case VCR_PAUSE:
            vcrMutex.unlock();
            usleep(1000 * 100);
            continue;
            break;

        case VCR_STOP:
            vcrMutex.unlock();
            clearAudioQ();
            usleep(1000 * 100);
            continue;
            break;

        default:
            vcrMutex.unlock();
            goto label1;
            break;
        }

label1:

        printf("audio\n");
        if (audioQueue->isEmpty())
        {
            qDebug() << "PlayAudio::play: GOT EMPTY";
            usleep(1000 * 10);
            continue;
        }

        printf("");
        pkt = audioQueue->dequeue();
        sendMutex->lock();
        send_audio_pkt(channel, stream_id, pkt->av_pkt);
        sendMutex->unlock();
        usleep(pkt->delay_in_us);
        delete pkt;
    }
}

void PlayAudio::setVcrOp(int op)
{
    vcrMutex.lock();
    this->vcrFlag = op;
    vcrMutex.unlock();
}

void PlayAudio::clearAudioQ()
{
    MediaPacket *pkt;

    while (!audioQueue->isEmpty())
    {
        pkt = audioQueue->dequeue();
        av_free_packet((AVPacket *) pkt->av_pkt);
        delete pkt;
    }
}
