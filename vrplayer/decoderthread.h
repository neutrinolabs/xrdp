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

#include <xrdpapi.h>
#include <xrdpvr.h>

/* ffmpeg related stuff */
extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

class DecoderThread : public QThread
{
    Q_OBJECT

public:
    DecoderThread();
    void setFilename(QString filename);

public slots:
    void on_geometryChanged(int x, int y, int width, int height);
    void on_mediaSeek(int value);

protected:
    void run();

private:
    typedef struct _VideoStateInfo
    {
        AVFormatContext *pFormatCtx;
    } VideoStateInfo;

    VideoStateInfo *vsi;
    QString         filename;
    void           *channel;
    int             stream_id;
    QRect           geometry;
    int64_t         elapsedTime; /* elapsed time in usecs since play started */
    QMutex          mutex;
    int64_t         la_seekPos;  /* locked access; must hold mutex */

    int openVirtualChannel();
    int closeVirtualChannel();
    int sendMetadataFile();
    int sendVideoFormat();
    int sendAudioFormat();
    int sendGeometry();

signals:
    void on_progressUpdate(int percent);
    void on_decoderErrorMsg(QString title, QString msg);
    void on_mediaDurationInSeconds(int duration);
    void on_elapsedtime(int val); /* in hundredth of a sec */
};

#endif // DECODERTHREAD_H
