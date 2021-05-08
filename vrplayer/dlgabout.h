#ifndef DLGABOUT_H
#define DLGABOUT_H

#include <QDialog>

namespace Ui
{
    class DlgAbout;
}

class DlgAbout : public QDialog
{
        Q_OBJECT

    public:
        explicit DlgAbout(QWidget *parent = 0);
        ~DlgAbout();

    private:
        Ui::DlgAbout *ui;

    private slots:
        void onOk();
};

#endif // DLGABOUT_H
