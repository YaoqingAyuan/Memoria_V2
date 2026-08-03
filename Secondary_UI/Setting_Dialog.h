#ifndef SETTING_DIALOG_H
#define SETTING_DIALOG_H

#include <QDialog>

namespace Ui {
class Setting_Dialog;
}

class Setting_Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Setting_Dialog(QWidget *parent = nullptr);
    ~Setting_Dialog();

private:
    Ui::Setting_Dialog *ui;
};

#endif // SETTING_DIALOG_H
