#ifndef WLAN_INPUT_WEIGHT_H
#define WLAN_INPUT_WEIGHT_H
//ADB无线导入窗口：通过adb.exe与Android设备通信，浏览并拉取缓存文件
//后端：AdbModule（设备发现/配对/连接/目录浏览/文件拉取）
//UI静态布局由 .ui 文件管理，运行时配置在 initUI() 中完成

#include <QWidget>
#include <QList>
#include <QString>

namespace Ui {
class WLAN_Input_Weight;
}

class QTreeWidgetItem;
class QStandardItemModel;
class QModelIndex;
class AdbModule;
struct AdbDeviceInfo;
struct AdbDirEntry;
class ParsedCacheData;

class WLAN_Input_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit WLAN_Input_Weight(QWidget *parent = nullptr);
    ~WLAN_Input_Weight();

signals:
    //确认导入：通知MainWindow将解析好的数据写入DataModel表格
    void importConfirmed();

private slots:
    //=== ADB设备管理 ===
    void onRefreshDevices();
    void onPairDevice();               //配对按钮 → 弹出配对对话框
    void onConnectDevice();            //连接按钮 → adb connect ip:port
    void onDeviceListChanged(const QList<AdbDeviceInfo> &devices);
    void onDeviceContextMenu(const QPoint &pos);
    void onDeviceSelectionChanged();   //设备列表选中变化 → 更新当前序列号

    //=== 远程文件浏览 ===
    void onFileDoubleClicked(const QModelIndex &index);   //双击目录 → 进入
    void onUpButtonClicked();           //上级目录
    void onDirListReady(const QString &path, const QList<AdbDirEntry> &entries);

    //=== 拉取+解析 ===
    void onParseSelected();             //解析选中 → adb pull + CacheFileParser
    void onPullProgressChanged(int percentage);
    void onPullFinished(const QString &localPath, bool success, const QString &message);

    //=== 预览/导入 ===
    void onConfirmImport();
    void onSelectAllChanged(bool checked);
    void onPreviewItemChanged(QTreeWidgetItem *item, int column);

    //=== ADB错误 ===
    void onAdbError(const QString &errorMsg);

private:
    void initUI();
    void updateAdbStatus(bool ready, const QString &text);
    void browseDir(const QString &remotePath);       //请求浏览指定远程目录
    void startNextPull();                             //从拉取队列取下一项执行
    void parsePulledFolder(const QString &localPath); //解析已拉取到本地的文件夹

    Ui::WLAN_Input_Weight *ui;
    AdbModule *m_adb;
    QStandardItemModel *m_fileModel;     //远程文件列表数据模型

    //ADB状态
    QString m_currentSerial;             //当前选中设备的序列号
    QString m_currentRemotePath;         //当前浏览的远程路径

    //拉取队列（ADB串行执行，一次只pull一个）
    QStringList m_pullQueue;             //待拉取的远程路径队列
    int m_pullTotal = 0;                 //本次批量拉取总数
    int m_pullCompleted = 0;             //已完成拉取数
    QString m_localPullDir;              //本地拉取暂存目录

    //B站缓存默认根路径
    static const QString BILI_CACHE_ROOT;
};

#endif // WLAN_INPUT_WEIGHT_H
