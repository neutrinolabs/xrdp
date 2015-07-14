#include "decoder.h"

Decoder::Decoder(QObject *parent) :
    QObject(parent)
{
    channel = NULL;
}

/*
 * inititialize the decoder
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int Decoder::init(QString filename)
{
    printf("Decoder::init\n");
    if (channel)
        return -1;

    /* open a virtual channel and connect to remote client */
    channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "xrdpvr", 0);
    if (channel == NULL)
    {
        qDebug() << "WTSVirtualChannelOpenEx() failed\n";
        return -1;
    }

    /* initialize the player */
    if (xrdpvr_init_player(channel, 101, filename.toAscii().data()))
    {
        fprintf(stderr, "failed to initialize the player\n");
        return -1;
    }

#if 1
    sleep(1);
    qDebug() << "sleeping 1";
    xrdpvr_set_geometry(channel, 101, mainWindowGeometry.x(),
                        mainWindowGeometry.y(), mainWindowGeometry.width(),
                        mainWindowGeometry.height());
    qDebug() << "set geometry to:" << mainWindowGeometry.x() <<
                "" << mainWindowGeometry.y() <<
                "" << mainWindowGeometry.width() <<
                "" << mainWindowGeometry.height();
#endif

    /* send compressed media data to client; client will decompress */
    /* the media and play it locally                                */
    xrdpvr_play_media(channel, 101, filename.toAscii().data());

    /* perform clean up */
    xrdpvr_deinit_player(channel, 101);

    WTSVirtualChannelClose(channel);

    return 0;
}

void Decoder::onGeometryChanged(QRect *g)
{
#if 1
    mainWindowGeometry.setX(g->x());
    mainWindowGeometry.setY(g->y());
    mainWindowGeometry.setWidth(g->width());
    mainWindowGeometry.setHeight(g->height());
#else
    if (!channel)
        return;

    xrdpvr_set_geometry(channel, 101, g->x(), g->y(), g->width(), g->height());
    qDebug() << "sent geometry";
#endif
}
