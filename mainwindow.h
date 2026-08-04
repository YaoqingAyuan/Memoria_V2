#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DataModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_OutputBtn_clicked();
    void on_OutputPath_Btn_clicked();
    void on_PlusLine_Btn_clicked();
    void on_DeleteLine_Btn_clicked();
    void on_IndepImport_Btn_clicked();
    void on_Setting_Btn_clicked();
    void on_Link_Input_Btn_clicked();
    void on_WLAN_Input_Btn_clicked();
    void on_LocalCache_Btn_clicked();

private:
    Ui::MainWindow *ui;

    DataModel *m_dataModel;     //表格数据模型

    //初始化表格(MetadataTable)的视觉属性：列宽、行高、选择行为等
    void initTable();
    //创建表格右键菜单(独立导入、删除行等)
    void setupTableContextMenu();
};

#endif // MAINWINDOW_H
