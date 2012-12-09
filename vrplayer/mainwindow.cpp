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
    ui->setupUi(this);
    acceptSliderMove = false;
    decoderThread = new DecoderThread();
    setupUI();

/* LK_TODO */
#if 0
    decoder = new Decoder(this);
    connect(this, SIGNAL(onGeometryChanged(int, int, int, int)),
            decoder, SLOT(onGeometryChanged(int, int, int, int)));
#endif

    /* register for signals/slots with decoderThread */
    connect(this, SIGNAL(on_geometryChanged(int,int,int,int)),
            decoderThread, SLOT(on_geometryChanged(int,int,int,int)));

    connect(decoderThread, SIGNAL(on_elapsedtime(int)),
            this, SLOT(on_elapsedTime(int)));

    connect(decoderThread, SIGNAL(on_decoderErrorMsg(QString, QString)),
            this, SLOT(on_decoderError(QString, QString)));

    connect(decoderThread, SIGNAL(on_mediaDurationInSeconds(int)),
            this, SLOT(on_mediaDurationInSeconds(int)));

    connect(this, SIGNAL(on_mediaSeek(int)), decoderThread, SLOT(on_mediaSeek(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    int rv;

    rv = QMessageBox::question(this, "Closing application",
                               "Do you really want to close vrplayer?",
                               QMessageBox::Yes | QMessageBox::No);

    if (rv == QMessageBox::No)
    {
        event->ignore();
        return;
    }
    decoderThread->exit(0);
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QRect rect;

    getVdoGeometry(&rect);
    emit on_geometryChanged(rect.x(), rect.y(), rect.width(), rect.height());
}

void MainWindow::moveEvent(QMoveEvent *e)
{
    QRect rect;

    getVdoGeometry(&rect);
    emit on_geometryChanged(rect.x(), rect.y(), rect.width(), rect.height());
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
    connect(slider, SIGNAL(actionTriggered(int)), this, SLOT(on_sliderActionTriggered(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(on_sliderValueChanged(int)));

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
    btnPlay = new QPushButton("P");
    btnPlay->setMinimumHeight(40);
    btnPlay->setMaximumHeight(40);
    btnPlay->setMinimumWidth(40);
    btnPlay->setMaximumWidth(40);
    connect(btnPlay, SIGNAL(clicked(bool)), this, SLOT(on_btnPlayClicked(bool)));

    /* setup stop button */
    btnStop = new QPushButton("S");
    btnStop->setMinimumHeight(40);
    btnStop->setMaximumHeight(40);
    btnStop->setMinimumWidth(40);
    btnStop->setMaximumWidth(40);

    /* setup rewind button */
    btnRewind = new QPushButton("R");
    btnRewind->setMinimumHeight(40);
    btnRewind->setMaximumHeight(40);
    btnRewind->setMinimumWidth(40);
    btnRewind->setMaximumWidth(40);

    /* add buttons to bottom panel */
    hboxLayoutBottom = new QHBoxLayout;
    hboxLayoutBottom->addWidget(btnPlay);
    hboxLayoutBottom->addWidget(btnStop);
    hboxLayoutBottom->addWidget(btnRewind);
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
        filename = QFileDialog::getOpenFileName(this, "Select Media File", "/home/lk/vbox_share");
    }
    else
    {
        /* show last selected file */
        filename = QFileDialog::getOpenFileName(this, "Select Media File",
                                                filename);
    }
    decoderThread->setFilename(filename);
}

void MainWindow::getVdoGeometry(QRect *rect)
{
    int x = geometry().x() + lblVideo->geometry().x();

    int y = pos().y() + lblVideo->geometry().y() +
            ui->mainToolBar->geometry().height() * 4 + 10;

    rect->setX(x);
    rect->setY(y);
    rect->setWidth(lblVideo->geometry().width());
    rect->setHeight(lblVideo->geometry().height());
}

/*******************************************************************************
 *                       actions and slots go here                             *
 ******************************************************************************/

void MainWindow::on_actionOpen_Media_File_triggered()
{
    openMediaFile();
}

void MainWindow::on_actionExit_triggered()
{
    /* TODO: confirm app exit */
    this->close();
}

void MainWindow::on_actionPlay_Media_triggered()
{
    // LK_TODO do we need this? if yes, should be same as on_btnPlayClicked()
#if 1
    decoderThread->start();
#else
    if (!decoder)
        return;

    decoder->init(filename);
#endif
}

void MainWindow::on_decoderError(QString title, QString msg)
{
    QMessageBox::information(this, title, msg);
}

void MainWindow::on_btnPlayClicked(bool flag)
{
    if (filename.length() == 0)
        openMediaFile();

    decoderThread->start();
}

void MainWindow::on_mediaDurationInSeconds(int duration)
{
    int  hours   = 0;
    int  minutes = 0;
    int  secs    = 0;
    char buf[20];

    /* setup progress bar */
    slider->setMinimum(0);
    slider->setMaximum(duration * 100); /* in hundredth of a sec */
    slider->setValue(0);

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
void MainWindow::on_elapsedTime(int val)
{
    int  hours    = 0;
    int  minutes  = 0;
    int  secs     = 0;
    int  duration = val / 100;
    char buf[20];

    /* if slider bar is down, do not update */
    if (slider->isSliderDown())
        return;

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

void MainWindow::on_sliderValueChanged(int value)
{
    if (acceptSliderMove)
    {
        acceptSliderMove = false;
        emit on_mediaSeek(value / 100);
    }
}

void MainWindow::on_sliderActionTriggered(int action)
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
void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    //qDebug() << "mouseMoveEvent: x=" << e->globalX() << "y=" << e->globalY();
}
#endif
