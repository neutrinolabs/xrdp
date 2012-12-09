#include "decoderthread.h"

DecoderThread::DecoderThread()
{
    vsi = NULL;
    channel = NULL;
    geometry.setX(0);
    geometry.setY(0);
    geometry.setWidth(0);
    geometry.setHeight(0);
    stream_id = 101;
    elapsedTime = 0;
    la_seekPos = 0;
}

void DecoderThread::run()
{
    int64_t start_time;
    int64_t duration;

    /* TODO what happens if we get called a 2nd time while we are still running */

    /* need a media file */
    if (filename.length() == 0)
    {
        emit on_decoderErrorMsg("No media file",
                                "Please select a media file to play");
        return;
    }

    /* connect to remote client */
    if (openVirtualChannel())
        return;

    vsi = (VideoStateInfo *) av_mallocz(sizeof(VideoStateInfo));
    if (vsi == NULL)
    {
        emit on_decoderErrorMsg("Resource error",
                                "Memory allocation failed, system out of memory");
        return;
    }

    /* register all formats/codecs */
    av_register_all();

    if (sendMetadataFile())
        return;

    if (sendVideoFormat())
        return;

    if (sendAudioFormat())
        return;

    if (sendGeometry())
        return;

    xrdpvr_play_media(channel, 101, filename.toAscii().data());

    xrdpvr_get_media_duration(&start_time, &duration);
    emit on_mediaDurationInSeconds(duration);

    qDebug() << "start_time=" << start_time << " duration=" << duration;

    while (xrdpvr_play_frame(channel, 101) == 0)
    {
        if (elapsedTime == 0)
            elapsedTime = av_gettime();

        /* time elapsed in 1/100th sec units since play started */
        emit on_elapsedtime((av_gettime() - elapsedTime) / 10000);

        mutex.lock();
        if (la_seekPos)
        {
            qDebug() << "seeking to" << la_seekPos;
            xrdpvr_seek_media(la_seekPos, 0);
            elapsedTime = av_gettime() - la_seekPos * 1000000;
            la_seekPos = 0;
        }
        mutex.unlock();
    }

    /* perform clean up */
    xrdpvr_deinit_player(channel, 101);

    /* clean up resources */
    closeVirtualChannel();
    if (vsi)
        av_free(vsi);
}

void DecoderThread::on_geometryChanged(int x, int y, int width, int height)
{
    geometry.setX(x);
    geometry.setY(y);
    geometry.setWidth(width);
    geometry.setHeight(height);

#if 0
    qDebug() << "decoderThread:signal" <<
                "" << geometry.x() <<
                "" << geometry.y() <<
                "" << geometry.width() <<
                "" << geometry.height();
#endif

    if (channel)
    {
        xrdpvr_set_geometry(channel, 101, geometry.x(), geometry.y(),
                            geometry.width(), geometry.height());
    }
}

void DecoderThread::on_mediaSeek(int value)
{
    mutex.lock();
    la_seekPos = value;
    mutex.unlock();
}

void DecoderThread::setFilename(QString filename)
{
    this->filename = filename;
}

/**
 * @brief Open a virtual connection to remote client
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int DecoderThread::openVirtualChannel()
{
    /* is channel already open? */
    if (channel)
        return -1;

    /* open a virtual channel and connect to remote client */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "xrdpvr", 0);
    if (channel == NULL)
    {
        emit on_decoderErrorMsg("Connection failure",
                                "Error connecting to remote client");
        return -1;
    }
    return 0;
}

int DecoderThread::closeVirtualChannel()
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
int DecoderThread::sendMetadataFile()
{
    if (xrdpvr_create_metadata_file(channel, filename.toAscii().data()))
    {
        emit on_decoderErrorMsg("I/O Error",
                                "An error occurred while sending data to remote client");
        return -1;
    }

    return 0;
}

int DecoderThread::sendVideoFormat()
{
    if (xrdpvr_set_video_format(channel, stream_id))
    {
        emit on_decoderErrorMsg("I/O Error",
                                "Error sending video format to remote client");
        return -1;
    }

    return 0;
}

int DecoderThread::sendAudioFormat()
{
    if (xrdpvr_set_audio_format(channel, stream_id))
    {
        emit on_decoderErrorMsg("I/O Error",
                                "Error sending audio format to remote client");
        return -1;
    }

    return 0;
}

int DecoderThread::sendGeometry()
{
    int rv;

    rv = xrdpvr_set_geometry(channel, stream_id, geometry.x(), geometry.y(),
                             geometry.width(), geometry.height());

    if (rv)
    {
        emit on_decoderErrorMsg("I/O Error",
                                "Error sending screen geometry to remote client");
        return -1;
    }
    return 0;
}
