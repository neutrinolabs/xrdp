#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <xrdpapi.h>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <QFileDialog>
#include <QDir>
#include <QListWidgetItem>
#include <QList>
#include <QMessageBox>
#include <QTimer>
#include <QStatusBar>
//#include <QDebug>

#include "utils.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow  *ui;
    void            *wtsChannel;
    QSystemTrayIcon *trayIcon;
    QMenu           *trayMenu;
    bool             okToQuit;
    QRect            savedGeometry;
    QStatusBar      *statusBar;

    QList<QListWidgetItem *> itemList;

    void setupSystemTray();
    int  initWtsChannel();
    int  deinitWtsChannel();
    void setStatusMsg(QString msg);
    void closeEvent(QCloseEvent * event);

private slots:
    void onBtnRefreshClicked();
    void onBtnUnmountClicked();
    void onActionQuit();
    void onActionLaunch();
    void onSystemTrayClicked(QSystemTrayIcon::ActivationReason);
};

#endif // MAINWINDOW_H
