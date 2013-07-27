#include "dlgabout.h"
#include "ui_dlgabout.h"

DlgAbout::DlgAbout(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgAbout)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(onOk()));
}

DlgAbout::~DlgAbout()
{
    delete ui;
}

void DlgAbout::onOk()
{
    this->done(0);
}
