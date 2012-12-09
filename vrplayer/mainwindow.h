#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

#include "decoder.h"
#include "decoderthread.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionOpen_Media_File_triggered();
    void on_actionExit_triggered();
    void on_actionPlay_Media_triggered();
    void on_decoderError(QString title, QString msg);
    void on_btnPlayClicked(bool flag);
    void on_mediaDurationInSeconds(int duration);
    void on_elapsedTime(int secs);
    void on_sliderActionTriggered(int value);
    void on_sliderValueChanged(int value);

signals:
    void on_geometryChanged(int x, int y, int widht, int height);
    void on_mediaSeek(int value);

protected:
    void resizeEvent(QResizeEvent *e);
    void closeEvent(QCloseEvent *e);
    void moveEvent(QMoveEvent *e);
    void mouseMoveEvent(QMouseEvent *e);

private:
    Ui::MainWindow *ui;

    QString        filename;
    Decoder       *decoder;
    DecoderThread *decoderThread;

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
    QWidget       *window;
    bool           acceptSliderMove;

    /* private methods */
    void setupUI();
    void openMediaFile();
    void getVdoGeometry(QRect *rect);
};

#endif // MAINWINDOW_H
