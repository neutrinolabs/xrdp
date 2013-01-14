#include "mainwindow.h"
#include "ui_mainwindow.h"

/*
 * TODO
 *      o should we use tick marks in QSlider?
 */

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    /* connect to remote client */
    interface = new OurInterface();
    if (interface->oneTimeInit())
    {
        oneTimeInitSuccess = false;

        /* connection to remote client failed; error msg has  */
        /* already been displayed so it's ok to close app now */
        QTimer::singleShot(1000, this, SLOT(close()));
    }
    else
    {
        oneTimeInitSuccess = true;
    }
    remoteClientInited = false;
    ui->setupUi(this);
    acceptSliderMove = false;
    setupUI();
    vcrFlag = 0;

    connect(this, SIGNAL(onGeometryChanged(int,int,int,int)),
            interface, SLOT(onGeometryChanged(int,int,int,int)));

    connect(interface, SIGNAL(onMediaDurationInSeconds(int)),
            this, SLOT(onMediaDurationInSeconds(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!oneTimeInitSuccess)
    {
        QMessageBox::warning(this, "Closing application",
                "This is not an xrdp session with xrdpvr");
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    QRect rect;

    getVdoGeometry(&rect);
    interface->sendGeometry(rect);
}

void MainWindow::moveEvent(QMoveEvent *)
{
    QRect rect;

    getVdoGeometry(&rect);
    interface->sendGeometry(rect);
}

void MainWindow::setupUI()
{
    /* setup area to display video */
    lblVideo = new QLabel();
    lblVideo->setMinimumWidth(320);
    lblVideo->setMinimumHeight(200);
    QPalette palette = lblVideo->palette();
    palette.setColor(lblVideo->backgroundRole(), Qt::black);
    palette.setColor(lblVideo->foregroundRole(), Qt::black);
    lblVideo->setAutoFillBackground(true);
    lblVideo->setPalette(palette);
    hboxLayoutTop = new QHBoxLayout;
    hboxLayoutTop->addWidget(lblVideo);

    /* setup label to display current pos in media */
    lblCurrentPos = new QLabel("00:00:00");
    lblCurrentPos->setMinimumHeight(20);
    lblCurrentPos->setMaximumHeight(20);

    /* setup slider to seek into media */
    slider = new QSlider();
    slider->setOrientation(Qt::Horizontal);
    slider->setMinimumHeight(20);
    slider->setMaximumHeight(20);
    connect(slider, SIGNAL(actionTriggered(int)), this, SLOT(onSliderActionTriggered(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderValueChanged(int)));

    /* setup label to display media duration */
    lblDuration = new QLabel("00:00:00");
    lblDuration->setMinimumHeight(20);
    lblDuration->setMaximumHeight(20);

    /* add above three widgets to mid layout */
    hboxLayoutMiddle = new QHBoxLayout;
    hboxLayoutMiddle->addWidget(lblCurrentPos);
    hboxLayoutMiddle->addWidget(slider);
    hboxLayoutMiddle->addWidget(lblDuration);

    /* setup play button */
    btnPlay = new QPushButton("Play");
    btnPlay->setMinimumHeight(40);
    btnPlay->setMaximumHeight(40);
    btnPlay->setMinimumWidth(40);
    btnPlay->setMaximumWidth(40);
    btnPlay->setCheckable(true);
    connect(btnPlay, SIGNAL(clicked(bool)),
            this, SLOT(onBtnPlayClicked(bool)));

    /* setup stop button */
    btnStop = new QPushButton("Stop");
    btnStop->setMinimumHeight(40);
    btnStop->setMaximumHeight(40);
    btnStop->setMinimumWidth(40);
    btnStop->setMaximumWidth(40);
    connect(btnStop, SIGNAL(clicked(bool)),
            this, SLOT(onBtnStopClicked(bool)));

    /* setup rewind button */
    btnRewind = new QPushButton("R");
    btnRewind->setMinimumHeight(40);
    btnRewind->setMaximumHeight(40);
    btnRewind->setMinimumWidth(40);
    btnRewind->setMaximumWidth(40);
    connect(btnRewind, SIGNAL(clicked(bool)),
            this, SLOT(onBtnRewindClicked(bool)));

    /* add buttons to bottom panel */
    hboxLayoutBottom = new QHBoxLayout;
    hboxLayoutBottom->addWidget(btnPlay);
    hboxLayoutBottom->addWidget(btnStop);
    //hboxLayoutBottom->addWidget(btnRewind);
    hboxLayoutBottom->addStretch();

    /* add all three layouts to one vertical layout */
    vboxLayout = new QVBoxLayout;
    vboxLayout->addLayout(hboxLayoutTop);
    vboxLayout->addLayout(hboxLayoutMiddle);
    vboxLayout->addLayout(hboxLayoutBottom);

    /* add all of them to central widget */
    window = new QWidget;
    window->setLayout(vboxLayout);
    this->setCentralWidget(window);
}

void MainWindow::openMediaFile()
{
    /* TODO take last stored value from QSettings */

    if (filename.length() == 0)
    {
        /* no previous selection - open user's home folder TODO */
        // TODO filename = QFileDialog::getOpenFileName(this, "Select Media File", "/");
        filename = QFileDialog::getOpenFileName(this, "Select Media File",
                                                QDir::currentPath());
    }
    else
    {
        /* show last selected file */
        filename = QFileDialog::getOpenFileName(this, "Select Media File",
                                                filename);
    }
    interface->setFilename(filename);
}

void MainWindow::getVdoGeometry(QRect *rect)
{
    int x;
    int y;
    int width;
    int height;
    QPoint pt;

    pt = lblVideo->mapToGlobal(QPoint(0, 0));
    x = pt.x();
    y = pt.y();
    width = lblVideo->width();
    height = lblVideo->height();
    rect->setX(x);
    rect->setY(y);
    rect->setWidth(width);
    rect->setHeight(height);
}

void MainWindow::clearDisplay()
{
    QPixmap pixmap(100,100);
    pixmap.fill(QColor(0x00, 0x00, 0x00));
    QPainter painter(&pixmap);
    painter.setBrush(QBrush(Qt::black));
    lblVideo->setPixmap(pixmap);
}

/*******************************************************************************
 *                       actions and slots go here                             *
 ******************************************************************************/

void MainWindow::on_actionOpen_Media_File_triggered()
{
    if (vcrFlag != 0)
        onBtnStopClicked(false);

    openMediaFile();
    if (filename.length() == 0)
    {
        /* cancel btn was clicked */
        return;
    }

    if (remoteClientInited)
    {
        remoteClientInited = false;
        interface->deInitRemoteClient();
        interface->initRemoteClient();
    }
    else
    {
        interface->initRemoteClient();
    }

    playVideo = interface->getPlayVideoInstance();
    if (playVideo)
    {
        connect(playVideo, SIGNAL(onElapsedtime(int)),
                this, SLOT(onElapsedTime(int)));
    }

    remoteClientInited = true;
    interface->playMedia();

    if (vcrFlag != 0)
    {
        interface->setVcrOp(VCR_PLAY);
        btnPlay->setText("Pause");
        vcrFlag = VCR_PLAY;
    }
}

void MainWindow::on_actionExit_triggered()
{
    clearDisplay();
    this->close();
}

void MainWindow::onBtnPlayClicked(bool)
{
    if (vcrFlag == 0)
    {
        /* first time play button has been clicked */
        on_actionOpen_Media_File_triggered();
        btnPlay->setText("Pause");
        vcrFlag = VCR_PLAY;
    }
    else if (vcrFlag == VCR_PLAY)
    {
        /* btn clicked while in play mode - enter pause mode */
        btnPlay->setText("Play");
        interface->setVcrOp(VCR_PAUSE);
        vcrFlag = VCR_PAUSE;
    }
    else if (vcrFlag == VCR_PAUSE)
    {
        /* btn clicked while in pause mode - enter play mode */
        btnPlay->setText("Pause");
        interface->setVcrOp(VCR_PLAY);
        vcrFlag = VCR_PLAY;
    }

    else if (vcrFlag == VCR_STOP)
    {
        /* btn clicked while stopped - enter play mode */
        btnPlay->setText("Play");
        interface->setVcrOp(VCR_PLAY);
        vcrFlag = VCR_PLAY;
    }
}

void MainWindow::onBtnRewindClicked(bool)
{
    if (playVideo)
        playVideo->onMediaSeek(0);
}

void MainWindow::onBtnStopClicked(bool)
{
    vcrFlag = VCR_STOP;
    btnPlay->setText("Play");
    interface->setVcrOp(VCR_STOP);

    /* reset slider */
    slider->setSliderPosition(0);
    lblCurrentPos->setText("00:00:00");

    /* clear screen by filling it with black */
    clearDisplay();
}

void MainWindow::onMediaDurationInSeconds(int duration)
{
    int  hours   = 0;
    int  minutes = 0;
    int  secs    = 0;
    char buf[20];

    /* setup progress bar */
    slider->setMinimum(0);
    slider->setMaximum(duration * 100); /* in hundredth of a sec */
    slider->setValue(0);
    slider->setSliderPosition(0);
    lblCurrentPos->setText("00:00:00");
    qDebug() << "media_duration=" << duration << " in hundredth of a sec:" << duration * 100;

    /* convert from seconds to hours:minutes:seconds */
    hours = duration / 3600;
    if (hours)
        duration -= (hours * 3600);

    minutes = duration / 60;
    if (minutes)
        duration -= minutes * 60;

    secs = duration;

    sprintf(buf, "%.2d:%.2d:%.2d", hours, minutes, secs);
    lblDuration->setText(QString(buf));
}

/**
 * time elapsed in 1/100th sec units since play started
 ******************************************************************************/
void MainWindow::onElapsedTime(int val)
{
    int  hours    = 0;
    int  minutes  = 0;
    int  secs     = 0;
    int  duration = val / 100;
    char buf[20];

    if (vcrFlag == VCR_STOP)
    {
        qDebug() << "onElapsedTime: not updating slider coz of VCR_STOP";
        return;
    }

    /* if slider bar is down, do not update */
    if (slider->isSliderDown())
    {
        qDebug() << "onElapsedTime: not updating slider coz slider is down";
        return;
    }

    /* update progress bar */
    slider->setSliderPosition(val);

    /* convert from seconds to hours:minutes:seconds */
    hours = duration / 3600;
    if (hours)
        duration -= (hours * 3600);

    minutes = duration / 60;
    if (minutes)
        duration -= minutes * 60;

    secs = duration;

    /* update current position in progress bar */
    sprintf(buf, "%.2d:%.2d:%.2d", hours, minutes, secs);
    lblCurrentPos->setText(QString(buf));
}

void MainWindow::onSliderValueChanged(int value)
{
    if (acceptSliderMove)
    {
        acceptSliderMove = false;
        if (playVideo)
            playVideo->onMediaSeek(value / 100);
    }
}

void MainWindow::onSliderActionTriggered(int action)
{
    switch (action)
    {
    case QAbstractSlider::SliderPageStepAdd:
        acceptSliderMove = true;
        break;

    case QAbstractSlider::SliderPageStepSub:
        acceptSliderMove = true;
        break;

    case QAbstractSlider::SliderMove:
        if (slider->isSliderDown())
            acceptSliderMove = true;
        break;
    }
}

#if 1
// LK_TODO delete this
void MainWindow::mouseMoveEvent(QMouseEvent *)
{
    //qDebug() << "mouseMoveEvent: x=" << e->globalX() << "y=" << e->globalY();
}
#endif
