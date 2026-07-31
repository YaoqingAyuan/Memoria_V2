#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_OutputBtn_clicked();

    void on_OutputPath_Btn_clicked();

    void on_VideoPath_Btn_clicked();

    void on_AudioPath_Btn_clicked();

    void on_PlusLine_Btn_clicked();

    void on_DeleteLine_Btn_clicked();

    void on_Setting_Btn_clicked();

    void on_Link_Input_Btn_clicked();

    void on_WLAN_Input_Btn_clicked();

    void on_LocalCache_Btn_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
