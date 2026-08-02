#ifndef INDEPEND_IMPORT_DIALOG_H
#define INDEPEND_IMPORT_DIALOG_H
//独立导入对话框：搭建模仿V1版UI，兼容.m4s散件导入

#include <QDialog>

namespace Ui {
class Independ_Import_Dialog;
}

class Independ_Import_Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Independ_Import_Dialog(QWidget *parent = nullptr);
    ~Independ_Import_Dialog();

private slots:
    void on_AudioPath_Btn_clicked();

    void on_VideoPath_Btn_clicked();

    void on_Center_Btn_clicked();

    void on_Cancel_Btn_clicked();

private:
    Ui::Independ_Import_Dialog *ui;
};

#endif // INDEPEND_IMPORT_DIALOG_H
