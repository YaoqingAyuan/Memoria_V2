#ifndef MAINWINDOW_H
#define MAINWINDOW_H
//主窗口
#include <QMainWindow>
#include <QMenu>
#include <QList>
#include "Core/ParsedCacheData.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DataModel;
class TaskQueue;
class ExterDevice_Input_Weight;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_OutputBtn_clicked();
    void on_OutputPath_Btn_clicked();
    void on_PlusLine_Btn_clicked();
    void on_DeleteLine_Btn_clicked();
    void on_IndepImport_Btn_clicked();
    void on_Setting_Btn_clicked();
    void on_ExterDevice_Input_Btn_clicked();
    void on_LocalCache_Btn_clicked();
    void onImportConfirmed(const QList<ParsedCacheData> &dataList);

    //单个导出任务完成：成功则删除对应缓存
    void onTaskFinished(int taskIndex, bool success, const QString &message);

private:
    Ui::MainWindow *ui;

    DataModel *m_dataModel;     //表格数据模型
    TaskQueue *m_taskQueue;     //导出任务队列(顺序驱动FFmpeg_module)

    ExterDevice_Input_Weight *m_exterDeviceInputWindow = nullptr;   //外部设备导入窗口(复用)
    QList<int> m_exportRowIndices;  //当前导出批次中任务索引→表格行索引的映射

    //初始化表格(MetadataTable)的视觉属性：列宽、行高、选择行为等
    void initTable();
    //创建表格右键菜单(独立导入、删除行等)
    void setupTableContextMenu();
    //删除指定行对应的ADB缓存（若该行数据来源于ADB导入）
    void deleteCacheForRow(int row);
};

#endif // MAINWINDOW_H
