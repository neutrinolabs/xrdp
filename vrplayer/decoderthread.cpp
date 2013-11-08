#include "decoderthread.h"

/*
 * TODO:
 *      o need to maintain aspect ratio while resizing
 *      o clicking in the middle of the slider bar shuld move the slider to the middle
 *      o need to be able to rewind the move when it is done playing
 *      o need to be able to load another move and play it w/o restarting player
 *      o pause button needs to work
 *      o need images for btns
 */

DecoderThread::DecoderThread()
{
    channel = NULL;
    geometry.setX(0);
    geometry.setY(0);
    geometry.setWidth(0);
    geometry.setHeight(0);
    stream_id = 101;
    elapsedTime = 0;
    la_seekPos = -1;
    videoTimer = NULL;
    audioTimer = NULL;
}

void DecoderThread::run()
{
    /* need a media file */
    if (filename.length() == 0)
    {
        emit on_decoderErrorMsg("No media file",
                                "Please select a media file to play");
        return;
    }
}

void DecoderThread::startMediaPlay()
{
    MediaPacket *mediaPkt;
    int          is_video_frame;
    int          rv;

    /* setup video timer; each time this timer fires, it sends        */
    /* one video pkt to the client then resets the callback duration  */
    videoTimer = new QTimer;
    connect(videoTimer, SIGNAL(timeout()), this, SLOT(videoTimerCallback()));
    //videoTimer->start(1500);

    /* setup audio timer; does the same as above, but with audio pkts */
    audioTimer = new QTimer;
    connect(audioTimer, SIGNAL(timeout()), this, SLOT(audioTimerCallback()));
    //audioTimer->start(500);

    /* setup pktTimer; each time this timer fires, it reads AVPackets */
    /* and puts them into audio/video Queues                          */
    pktTimer = new QTimer;
    connect(pktTimer, SIGNAL(timeout()), this, SLOT(pktTimerCallback()));

    while (1)
    {
        /* fill the audio/video queues with initial data; thereafter  */
        /* data will be filled by pktTimerCallback()                  */
        if ((audioQueue.count() >= 3000) || (videoQueue.count() >= 3000))
        {
            //pktTimer->start(50);

            //videoTimer->start(1500);
            //audioTimer->start(500);

            playVideo = new PlayVideo(NULL, &videoQueue, channel, 101);
            playVideoThread = new QThread(this);
            connect(playVideoThread, SIGNAL(started()), playVideo, SLOT(play()));
            playVideo->moveToThread(playVideoThread);
            playVideoThread->start();

            playAudio = new PlayAudio(NULL, &audioQueue, channel, 101);
            playAudioThread = new QThread(this);
            connect(playAudioThread, SIGNAL(started()), playAudio, SLOT(play()));
            playAudio->moveToThread(playAudioThread);
            playAudioThread->start();

            return;
        }

        mediaPkt = new MediaPacket;
        rv = xrdpvr_get_frame(&mediaPkt->av_pkt,
                              &is_video_frame,
                              &mediaPkt->delay_in_us);
        if (rv < 0)
        {
            /* looks like we reached end of file */
            break;
        }

        if (is_video_frame)
            videoQueue.enqueue(mediaPkt);
        else
            audioQueue.enqueue(mediaPkt);

    } /* end while (1) */
}

void DecoderThread::on_mediaSeek(int value)
{
    mutex.lock();
    la_seekPos = value;
    mutex.unlock();

    qDebug() << "media seek value=" << value;

    /* pktTimer stops at end of media; need to restart it */
    if (!pktTimer->isActive())
    {
        updateSlider();
        pktTimer->start(100);
    }
}

void DecoderThread::setFilename(QString filename)
{
    this->filename = filename;
}

void DecoderThread::stopPlayer()
{
    pktTimer->stop();
    audioQueue.clear();
    videoQueue.clear();
}

void DecoderThread::pausePlayer()
{
    pktTimer->stop();
}

void DecoderThread::resumePlayer()
{
    pktTimer->start(100);
}

void DecoderThread::close()
{
}

void DecoderThread::audioTimerCallback()
{
    MediaPacket *pkt;
    int          delayInMs;

    if (audioQueue.isEmpty())
    {
        qDebug() << "audioTimerCallback: got empty";
        audioTimer->setInterval(100);
        return;
    }

    printf("audio2\n");
    pkt = audioQueue.dequeue();
    delayInMs = (int) ((float) pkt->delay_in_us / 1000.0);
    send_audio_pkt(channel, 101, pkt->av_pkt);
    delete pkt;

    //qDebug() << "audioTimerCallback: delay :" << delayInMs;

    audioTimer->setInterval(delayInMs);
}

void DecoderThread::videoTimerCallback()
{
    MediaPacket *pkt;
    int          delayInMs;

    if (videoQueue.isEmpty())
    {
        qDebug() << "videoTimerCallback: GOT EMPTY";
        videoTimer->setInterval(100);
        return;
    }

    pkt = videoQueue.dequeue();
    delayInMs = (int) ((float) pkt->delay_in_us / 1000.0);
    send_video_pkt(channel, 101, pkt->av_pkt);
    delete pkt;
    updateSlider();
    //qDebug() << "videoTimerCallback: delay :" << delayInMs;
    videoTimer->setInterval(delayInMs);
}

void DecoderThread::pktTimerCallback()
{
    MediaPacket *mediaPkt;
    int          is_video_frame;
    int          rv;

    while (1)
    {
        qDebug() << "pktTimerCallback: audioCount=" <<  audioQueue.count() << "videoCount=" << videoQueue.count();
#if 1
        if ((audioQueue.count() >= 20) || (videoQueue.count() >= 20))
            return;
#else
        if (videoQueue.count() >= 60)
            return;
#endif
        mediaPkt = new MediaPacket;
        rv = xrdpvr_get_frame(&mediaPkt->av_pkt,
                              &is_video_frame,
                              &mediaPkt->delay_in_us);
        if (rv < 0)
        {
            /* looks like we reached end of file */
            qDebug() << "###### looks like we reached EOF";
            pktTimer->stop();
            // LK_TODO set some flag so audio/video timer also stop when q is empty
            return;
        }

        if (is_video_frame)
            videoQueue.enqueue(mediaPkt);
        else
            audioQueue.enqueue(mediaPkt);
    }
}

void DecoderThread::updateSlider()
{
    if (elapsedTime == 0)
        elapsedTime = av_gettime();

    /* time elapsed in 1/100th sec units since play started */
    emit on_elapsedtime((av_gettime() - elapsedTime) / 10000);

    mutex.lock();
    if (la_seekPos >= 0)
    {
        qDebug() << "seeking to" << la_seekPos;
        //audioTimer->stop();
        //videoTimer->stop();
        xrdpvr_seek_media(la_seekPos, 0);
        elapsedTime = av_gettime() - la_seekPos * 1000000;
        //audioTimer->start(10);
        //videoTimer->start(10);
        la_seekPos = -1;
    }
    mutex.unlock();
}
