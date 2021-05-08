#ifndef DECODER_H
#define DECODER_H

#include <QObject>
#include <QDebug>
#include <QRect>

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

#include <libavformat/avformat.h>
#include <xrdpapi.h>
#include <xrdpvr.h> /* LK_TODO is this required? */

class Decoder : public QObject
{
        Q_OBJECT
    public:
        explicit Decoder(QObject *parent = 0);
        int init(QString filename);
        //int deinit();
        //int setWindow(QRectangle rect);

    private:
        void *channel;
        QRect mainWindowGeometry;

    signals:

    public slots:
        void onGeometryChanged(QRect *geometry);
};

#endif // DECODER_H
