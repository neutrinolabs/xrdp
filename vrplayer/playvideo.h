#ifndef PLAYVIDEO_H
#define PLAYVIDEO_H

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
#include "xrdpapi.h"

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

class PlayVideo : public QObject
{
        Q_OBJECT

    public:
        explicit PlayVideo(QObject *parent = 0,
                           QQueue<MediaPacket *> *videoQueue = 0,
                           QMutex *sendMutex = 0,
                           void *channel = 0,
                           int stream_id = 101,
                           int fps = 24);

        //void onMediaSeek(int value);
        //void setVcrOp(int op);
        //void onMediaRestarted();

    public slots: // cppcheck-suppress unknownMacro
        void play();

        //signals:
        //    void onElapsedtime(int val); /* in hundredth of a sec */

    private:
        QQueue<MediaPacket *> *videoQueue;

        //    int       vcrFlag;
        //    QMutex    vcrMutex;
        QMutex   *sendMutex;
        //    QMutex    posMutex;
        //    int64_t   la_seekPos;  /* locked access; must hold posMutex */
        void     *channel;
        int       stream_id;
        int       fps;
        //    int64_t   elapsedTime; /* elapsed time in usecs since play started */
        //    int64_t   pausedTime;  /* time at which stream was paused          */
        //    bool      isStopped;

        //    void updateMediaPos();
        //    void clearVideoQ();
};

#endif // PLAYVIDEO_H
