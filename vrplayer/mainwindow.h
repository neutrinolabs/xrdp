#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif

#include <QMainWindow>
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QPoint>
#include <QRect>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <QTimer>

#include "xrdpapi.h"
#include "xrdpvr.h"
#include "decoder.h"
#include "ourinterface.h"
#include "playvideo.h"
#include "dlgabout.h"

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

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();

    signals:
        void onGeometryChanged(int x, int y, int width, int height);

    public slots:
        void onSliderValueChanged(int value);

    private slots:
        void on_actionOpen_Media_File_triggered();
        void on_actionExit_triggered();

        void onBtnPlayClicked(bool flag);
        void onBtnRewindClicked(bool flag);
        void onBtnStopClicked(bool flag);

        void onMediaDurationInSeconds(int duration);
        void onElapsedTime(int secs);
        void onSliderActionTriggered(int value);
        void onMoveCompleted();

        void on_actionAbout_triggered();

        void onVolSliderValueChanged(int value);

    protected:
        void resizeEvent(QResizeEvent *e);
        void closeEvent(QCloseEvent *e);
        void moveEvent(QMoveEvent *e);

    private:
        Ui::MainWindow *ui;

        /* for UI */
        QLabel        *lblCurrentPos;
        QLabel        *lblDuration;
        QLabel        *lblVideo;
        QHBoxLayout   *hboxLayoutTop;
        QHBoxLayout   *hboxLayoutMiddle;
        QHBoxLayout   *hboxLayoutBottom;
        QVBoxLayout   *vboxLayout;
        QPushButton   *btnPlay;
        QPushButton   *btnStop;
        QPushButton   *btnRewind;
        QSlider       *slider;
        QSlider       *volSlider;
        QWidget       *window;
        bool           acceptSliderMove;
        QTimer        *moveResizeTimer;

        /* private stuff */
        OurInterface  *interface;
        //PlayVideo     *playVideo;
        DemuxMedia    *demuxMedia;
        QString        filename;
        bool           oneTimeInitSuccess;
        bool           remoteClientInited;
        void          *channel;
        int            stream_id;
        int64_t        elapsedTime; /* elapsed time in usecs since play started */
        int            vcrFlag;
        bool           gotMediaOnCmdline;

        /* private methods */
        void setupUI();
        void openMediaFile();
        void getVdoGeometry(QRect *rect);
        void clearDisplay();
};

#endif // MAINWINDOW_H
