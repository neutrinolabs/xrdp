#ifndef DEMUXMEDIA_H
#define DEMUXMEDIA_H

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

#include <QObject>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QDebug>

#include "mediapacket.h"
#include "playaudio.h"
#include "playvideo.h"

/* ffmpeg related stuff */
extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#define VCR_PLAY        1
#define VCR_PAUSE       2
#define VCR_STOP        3
#define VCR_REWIND      4
#define VCR_POWER_OFF   5

class DemuxMedia : public QObject
{
    Q_OBJECT
public:
    explicit DemuxMedia(QObject *parent = 0, QQueue<MediaPacket *> *audioQueue = 0,
                        QQueue<MediaPacket *> *videoQueue = 0, void *channel = 0, int stream_id = 101);

    void setVcrOp(int op);

public slots:
    void       startDemuxing();
    PlayVideo *getPlayVideoInstance();

private:
    QQueue<MediaPacket *> *audioQueue;
    QQueue<MediaPacket *> *videoQueue;
    QMutex                 vcrMutex;
    int                    vcrFlag;
    void                  *channel;
    PlayVideo             *playVideo;
    QThread               *playVideoThread;
    PlayAudio             *playAudio;
    QThread               *playAudioThread;
    int                    stream_id;
    bool                   threadsStarted;
    QMutex                 sendMutex;

    void startAudioVideoThreads();
};

#endif // DEMUXMEDIA_H
