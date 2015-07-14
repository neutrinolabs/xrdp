#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    wtsChannel = NULL;
    okToQuit = false;
    savedGeometry = this->geometry();

    /* setup tab to unmount drives */
    ui->tabWidget->setTabText(0, "Unmount drives");
    connect(ui->btnRefresh, SIGNAL(clicked()), this, SLOT(onBtnRefreshClicked()));
    connect(ui->btnUnmount, SIGNAL(clicked()), this, SLOT(onBtnUnmountClicked()));

    ui->tabWidget->setTabText(1, "");
    if (initWtsChannel())
    {
        okToQuit = true;
        QTimer::singleShot(10, qApp, SLOT(quit()));
        return;
    }
    setupSystemTray();

    /* set up status bar to display messages */
    statusBar = new QStatusBar;
    this->setStatusBar(statusBar);
    setStatusMsg("Connected to client");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupSystemTray()
{
    trayMenu = new QMenu(this);
    trayMenu->addAction("Launch Thinclient Utils", this, SLOT(onActionLaunch()));
    trayMenu->addSeparator();
    trayMenu->addAction("Quit", this, SLOT(onActionQuit()));

    trayIcon = new QSystemTrayIcon;
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setIcon(QIcon(":/images/resources/images/tools.gif"));

    trayIcon->show();

    trayIcon->showMessage("TCutils", "Click on the tcutils icon to launch tcutils",
                          QSystemTrayIcon::Information, 3000);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(onSystemTrayClicked(QSystemTrayIcon::ActivationReason)));
}

/**
 *
 *****************************************************************************/

int MainWindow::initWtsChannel()
{
    /* init the channel just once */
    if (wtsChannel)
        return 0;

    /* open a WTS channel and connect to remote client */
    wtsChannel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "tcutils", 0);
    if (wtsChannel == NULL)
    {
        QMessageBox::information(this, "Open virtual channel", "Error "
                                 "connecting to remote client. This program "
                                 "can only be used when connected via "
                                 "NeutrinoRDP.\n\nClick ok to close this "
                                 "application");
        return -1;
    }

    return 0;
}

/**
 *
 *****************************************************************************/

int MainWindow::deinitWtsChannel()
{
    if (!wtsChannel)
        return -1;

    WTSVirtualChannelClose(wtsChannel);
    return 0;
}

/**
 * Display a msg on the status bar
 *
 * @param  msg  message to display in status bar
 *****************************************************************************/

void MainWindow::setStatusMsg(QString msg)
{
    statusBar->showMessage(msg, 30000);
}

/**
 *
 *****************************************************************************/

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!okToQuit)
    {
        savedGeometry = this->geometry();
        this->hide();
        event->ignore();
        return;
    }

    if (wtsChannel)
        deinitWtsChannel();

    event->accept();
}

/******************************************************************************
**                                                                           **
**                              slots go here                                **
**                                                                           **
******************************************************************************/
#if 1
void MainWindow::onBtnRefreshClicked()
{
    int i;

    /* clear drive list */
    if (itemList.count())
    {
        for (i = 0; i < itemList.count(); i++)
            ui->listWidget->removeItemWidget(itemList.at(i));

        itemList.clear();
    }
    ui->listWidget->clear();

    if (Utils::getMountList(wtsChannel, &itemList))
    {
        QMessageBox::information(this, "Get device list", "\nError getting "
                                 "device list from client");
        return;
    }

    if (itemList.count() == 0)
    {
        QMessageBox::information(this, "Get device list",
                                 "\nNo devices found!");
        return;
    }

    /* add mount point to list widget */
    for (i = 0; i < itemList.count(); i++)
        ui->listWidget->insertItem(i, itemList.at(i));
}
#else
void MainWindow::onBtnRefreshClicked()
{
    QListWidgetItem *item;
    int              i;
    int              j;

    /* clear drive list */
    if (itemList.count())
    {
        for (i = 0; i < itemList.count(); i++)
            ui->listWidget->removeItemWidget(itemList.at(i));

        itemList.clear();
    }
    ui->listWidget->clear();

    QDir dir(QDir::homePath() + "/xrdp_client");
    QStringList sl = dir.entryList();

    for (i = 0, j = 0; i < sl.count(); i++)
    {
        /* skip files starting with . */
        if (sl.at(i).startsWith("."))
            continue;

        /* add mount point to list widget */
        item = new QListWidgetItem;
        item->setText(sl.at(i));
        ui->listWidget->insertItem(j++, item);
        itemList.append(item);
    }
}
#endif

void MainWindow::onBtnUnmountClicked()
{
    QListWidgetItem *item = ui->listWidget->currentItem();

    if (!item)
    {
        QMessageBox::information(this, "Unmount device", "\nNo device selected. "
                                 "You must select a device to unmount");
        return;
    }

    if (Utils::unmountDevice(wtsChannel, item->text(), statusBar) == 0)
    {
        delete ui->listWidget->takeItem(itemList.indexOf(item));
        itemList.removeOne(item);
    }
}

void MainWindow::onActionQuit()
{
    okToQuit = true;
    this->close();
}

void MainWindow::onActionLaunch()
{
    this->show();
    this->setGeometry(savedGeometry);
}

void MainWindow::onSystemTrayClicked(QSystemTrayIcon::ActivationReason)
{
    trayMenu->popup(QCursor::pos());
}
