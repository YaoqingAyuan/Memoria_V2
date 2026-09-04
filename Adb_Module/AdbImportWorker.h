#ifndef ADBIMPORTWORKER_H
#define ADBIMPORTWORKER_H
//ADB导入工作器：封装拉取队列管理与缓存文件解析
//从 ExterDevice_Input_Weight 中提取，UI类通过信号更新预览树

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "core/ParsedCacheData.h"

class AdbModule;

class AdbImportWorker : public QObject
{
    Q_OBJECT
public:
    explicit AdbImportWorker(AdbModule *adb, QObject *parent = nullptr);

    //启动批量拉取+解析
    void startPull(const QStringList &remotePaths, const QString &serial, const QString &localDir);

    //取出所有已解析的数据(供UI导入使用)
    QMap<QString, QList<ParsedCacheData>> takeParsedData();

signals:
    //单个文件夹拉取进度
    void pullProgressChanged(int percentage);

    //开始拉取单个文件夹
    void pullStarted(int current, int total, const QString &folderName);

    //单个文件夹解析完成(成功或失败)
    void folderParsed(const QString &folderName, const QList<ParsedCacheData> &parsedList,
                      const QString &localPath, bool success);

    //全部拉取完成
    void pullCompleted(int completed, int total);

private slots:
    void onPullFinished(const QString &localPath, bool success, const QString &message);

private:
    void startNextPull();
    void parsePulledFolder(const QString &localPath);

    AdbModule *m_adb;
    QStringList m_pullQueue;
    int m_pullTotal = 0;
    int m_pullCompleted = 0;
    QString m_localPullDir;
    QString m_currentSerial;
    QMap<QString, QList<ParsedCacheData>> m_parsedData;
};

#endif // ADBIMPORTWORKER_H
