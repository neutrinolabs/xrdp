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
        explicit DemuxMedia(QObject *parent = 0, QQueue<MediaPacket *> *videoQueue = 0,
                            void *channel = 0, int stream_id = 101);

        void setVcrOp(int op);
        int clear();

    public slots:
        void       startDemuxing();
        void       onMediaSeek(int value);

    private:
        QMutex     vcrMutex;
        int        vcrFlag;
        void      *channel;
        int        stream_id;
        QMutex     sendMutex;
        QMutex     posMutex;
        int64_t    elapsedTime; /* elapsed time in usecs since play started */
        int64_t    pausedTime;  /* time at which stream was paused          */
        int64_t    la_seekPos;  /* locked access; must hold posMutex        */
        bool       isStopped;

        QQueue<MediaPacket *> *videoQueue;
        PlayVideo             *playVideo;
        QThread               *playVideoThread;

        void updateMediaPos();

    signals:
        void onMediaRestarted();

    signals:
        void onElapsedtime(int val); /* in hundredth of a sec */

};

#endif // DEMUXMEDIA_H
