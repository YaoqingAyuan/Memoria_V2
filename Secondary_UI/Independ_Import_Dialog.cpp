#include "Independ_Import_Dialog.h"
#include "./ui_Independ_Import_Dialog.h"
#include <QFileDialog>
#include <QMessageBox>

Independ_Import_Dialog::Independ_Import_Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Independ_Import_Dialog)
{
    ui->setupUi(this);
}

Independ_Import_Dialog::~Independ_Import_Dialog()
{
    delete ui;
}


//音频路径浏览按钮：选择.m4s音频文件，填入AudioPath_Edit
void Independ_Import_Dialog::on_AudioPath_Btn_clicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择音频文件"),
        QString(),
        QStringLiteral("M4S 文件 (*.m4s)"));
    if (path.isEmpty())
        return;
    ui->AudioPath_Edit->setText(path);
}


//视频路径浏览按钮：选择.m4s视频文件，填入VideoPath_Edit
void Independ_Import_Dialog::on_VideoPath_Btn_clicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择视频文件"),
        QString(),
        QStringLiteral("M4S 文件 (*.m4s)"));
    if (path.isEmpty())
        return;
    ui->VideoPath_Edit->setText(path);
}


//确定按钮：校验路径非空后关闭对话框(返回Accepted，由主窗口读取数据)
void Independ_Import_Dialog::on_Center_Btn_clicked()
{
    const QString audio = ui->AudioPath_Edit->text().trimmed();
    const QString video = ui->VideoPath_Edit->text().trimmed();

    //校验：音频和视频路径都不可为空
    if (audio.isEmpty() || video.isEmpty()) {
        QMessageBox::warning(this,
            QStringLiteral("提示"),
            QStringLiteral("请同时选择音频和视频文件路径"));
        return;
    }

    accept();
}


//取消按钮：关闭对话框(返回Rejected)
void Independ_Import_Dialog::on_Cancel_Btn_clicked()
{
    reject();
}


//=== 数据获取(供主窗口在Accepted后读取) ===

QString Independ_Import_Dialog::audioPath() const
{
    return ui->AudioPath_Edit->text().trimmed();
}

QString Independ_Import_Dialog::videoPath() const
{
    return ui->VideoPath_Edit->text().trimmed();
}

QString Independ_Import_Dialog::title() const
{
    return ui->Title_Edit->text().trimmed();
}
