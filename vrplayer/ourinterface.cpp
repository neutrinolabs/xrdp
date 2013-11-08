
#include "ourinterface.h"

OurInterface::OurInterface(QObject *parent) :
    QObject(parent)
{
    channel = NULL;
    savedGeometry.setX(0);
    savedGeometry.setY(0);
    savedGeometry.setWidth(0);
    savedGeometry.setHeight(0);
    stream_id = 101;
    demuxMedia = 0;
    //elapsedTime = 0;
}

int OurInterface::oneTimeInit()
{
    /* connect to remote client */
    if (openVirtualChannel())
        return -1;

    /* register all formats/codecs */
    av_register_all();

    return 0;
}

void OurInterface::oneTimeDeinit()
{
    /* clean up resources */
    closeVirtualChannel();
}

void OurInterface::initRemoteClient()
{
    int64_t start_time;
    int64_t duration;

    //elapsedTime = 0;

    if (sendMetadataFile())
        return;

    if (sendVideoFormat())
        return;

    if (sendAudioFormat())
        return;

    if (sendGeometry(savedGeometry))
        return;

    xrdpvr_play_media(channel, 101, filename.toAscii().data());

    xrdpvr_get_media_duration(&start_time, &duration);
    //qDebug() << "ourInterface:initRemoteClient: emit onMediaDurationInSecs: dur=" << duration;
    emit onMediaDurationInSeconds(duration);

    /* LK_TODO this needs to be undone in deinitRemoteClient() */
    if (!demuxMedia)
    {
        demuxMedia = new DemuxMedia(NULL, &audioQueue, &videoQueue, channel, stream_id);
        demuxMediaThread = new QThread(this);
        connect(demuxMediaThread, SIGNAL(started()), demuxMedia, SLOT(startDemuxing()));
        demuxMedia->moveToThread(demuxMediaThread);
        playVideo = demuxMedia->getPlayVideoInstance();
    }
}

void OurInterface::deInitRemoteClient()
{
    /* perform clean up */
    xrdpvr_deinit_player(channel, 101);
}

/**
 * @brief Open a virtual connection to remote client
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int OurInterface::openVirtualChannel()
{
    /* is channel already open? */
    if (channel)
        return -1;

    printf("OurInterface::openVirtualChannel:\n");
    /* open a virtual channel and connect to remote client */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "xrdpvr", 0);
    if (channel == NULL)
    {

        emit on_ErrorMsg("Connection failure",
                         "Error connecting to remote client. Application will close now");
        return -1;
    }
    return 0;
}

int OurInterface::closeVirtualChannel()
{
    /* channel must be opened first */
    if (!channel)
        return -1;

    WTSVirtualChannelClose(channel);
    return 0;
}

/**
 * @brief this is a temp hack while we figure out how to set up the right
 *        context for avcodec_decode_video2() on the server side; the workaround
 *        is to send the first 1MB of the media file to the server end which
 *        reads this file and sets up its context
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int OurInterface::sendMetadataFile()
{

    if (xrdpvr_init_player(channel, 101, filename.toAscii().data()))
    {
        fprintf(stderr, "failed to initialize the player\n");
        return -1;
    }
#if 0
    if (xrdpvr_create_metadata_file(channel, filename.toAscii().data()))
    {
        emit on_ErrorMsg("I/O Error",
                         "An error occurred while sending data to remote client");
        return -1;
    }
#endif
    return 0;
}

int OurInterface::sendVideoFormat()
{
#if 0
    if (xrdpvr_set_video_format(channel, stream_id))
    {
        emit on_ErrorMsg("I/O Error",
                         "Error sending video format to remote client");
        return -1;
    }
#endif
    return 0;
}

int OurInterface::sendAudioFormat()
{
#if 0
    if (xrdpvr_set_audio_format(channel, stream_id))
    {
        emit on_ErrorMsg("I/O Error",
                         "Error sending audio format to remote client");
        return -1;
    }
#endif
    return 0;
}

int OurInterface::sendGeometry(QRect rect)
{
    int rv;

    savedGeometry.setX(rect.x());
    savedGeometry.setY(rect.y());
    savedGeometry.setWidth(rect.width());
    savedGeometry.setHeight(rect.height());

    rv = xrdpvr_set_geometry(channel, stream_id, savedGeometry.x(),
                             savedGeometry.y(), savedGeometry.width(),
                             savedGeometry.height());

    if (rv)
    {
        emit on_ErrorMsg("I/O Error",
                         "Error sending screen geometry to remote client");
        return -1;
    }

    return 0;
}

void OurInterface::onGeometryChanged(int x, int y, int width, int height)
{
    savedGeometry.setX(x);
    savedGeometry.setY(y);
    savedGeometry.setWidth(width);
    savedGeometry.setHeight(height);

#if 0
    qDebug() << "OurInterface:signal" <<
                "" << savedGeometry.x() <<
                "" << savedGeometry.y() <<
                "" << savedGeometry.width() <<
                "" << savedGeometry.height();
#endif

    if (channel)
    {
        xrdpvr_set_geometry(channel, 101, savedGeometry.x(), savedGeometry.y(),
                            savedGeometry.width(), savedGeometry.height());
    }
}

void OurInterface::setFilename(QString filename)
{
    this->filename = filename;
}

void OurInterface::playMedia()
{
    demuxMediaThread->start();
}

PlayVideo * OurInterface::getPlayVideoInstance()
{
    return this->playVideo;
}

void OurInterface::setVcrOp(int op)
{
    if (demuxMedia)
        demuxMedia->setVcrOp(op);
}

int OurInterface::setVolume(int volume)
{
    printf("OurInterface::setVolume\n");
    if (xrdpvr_set_volume(channel, volume))
    {
        emit on_ErrorMsg("I/O Error",
                         "Error sending volume to remote client");
        return -1;
    }
    return 0;
}
