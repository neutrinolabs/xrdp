#ifndef PLAYAUDIO_H
#define PLAYAUDIO_H

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

#include <QObject>
#include <QQueue>
#include <QMutex>

#include "mediapacket.h"
#include "xrdpvr.h"

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

class PlayAudio : public QObject
{
        Q_OBJECT

    public:
        explicit PlayAudio(QObject *parent = 0,
                           QQueue<MediaPacket *> *audioQueue = 0,
                           QMutex *sendMutex = 0,
                           void *channel = 0,
                           int stream_id = 101);

        void setVcrOp(int op);

    public slots: // cppcheck-suppress unknownMacro
        void play();

    private:
        QQueue<MediaPacket *> *audioQueue;
        QMutex                *sendMutex;
        QMutex                 vcrMutex;
        int                    vcrFlag;
        void                  *channel;
        int                    stream_id;

        void clearAudioQ();
};

#endif // PLAYAUDIO_H
