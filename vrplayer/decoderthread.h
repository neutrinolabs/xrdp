#ifndef DECODERTHREAD_H
#define DECODERTHREAD_H

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

#include <QThread>
#include <QDebug>
#include <QString>
#include <QRect>
#include <QMutex>
#include <QTimer>
#include <QQueue>

#include <xrdpapi.h>
#include <xrdpvr.h>
#include <mediapacket.h>
#include <playvideo.h>
#include <playaudio.h>

/* ffmpeg related stuff */
extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

class DecoderThread : public QObject
{
    Q_OBJECT

public:
    /* public methods */
    DecoderThread();

    void setFilename(QString filename);
    void stopPlayer();
    void pausePlayer();
    void resumePlayer();
    void oneTimeDeinit();
    void close();
    void run();
    void startMediaPlay();

public slots:
    void on_mediaSeek(int value);

private:
    /* private variables */
    QQueue<MediaPacket *> audioQueue;
    QQueue<MediaPacket *> videoQueue;

    QTimer         *videoTimer;
    QTimer         *audioTimer;
    QTimer         *pktTimer;
    QString         filename;
    void           *channel;
    int             stream_id;
    QRect           geometry;
    int64_t         elapsedTime; /* elapsed time in usecs since play started */
    QMutex          mutex;
    int64_t         la_seekPos;  /* locked access; must hold mutex */

    PlayVideo      *playVideo;
    QThread        *playVideoThread;
    PlayAudio      *playAudio;
    QThread        *playAudioThread;

    /* private functions */
    int  sendMetadataFile();
    int  sendAudioFormat();
    int  sendVideoFormat();
    int  sendGeometry();
    void updateSlider();

private slots:
    /* private slots */
    void audioTimerCallback();
    void videoTimerCallback();
    void pktTimerCallback();

signals:
    /* private signals */
    void on_progressUpdate(int percent);
    void on_decoderErrorMsg(QString title, QString msg);
    void on_mediaDurationInSeconds(int duration);
    void on_elapsedtime(int val); /* in hundredth of a sec */
};

#endif // DECODERTHREAD_H
