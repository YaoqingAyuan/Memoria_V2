#ifndef WLANMANAGER_H
#define WLANMANAGER_H
//顶层协调器：拥有发现层 + 传输服务端，按需创建传输客户端
//桥接网络事件到 UI 与现有数据主线(CacheFileParser→ParsedCacheData→DataModel)
//对应原理图的 ImportSession（改名以体现"协调者"角色）
//
//这是唯一与 MainWindow/ExterDevice_Input_Weight 直接对话的类：
//  - 接收侧：discovery + server 信号汇总后对外暴露
//  - 发送侧：sendToPeer 创建临时 client
//  - 收齐一批文件后 emit importBatchDone(顶层目录)，供主窗口走解析主线

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include "WlanTypes.h"

class DiscoveryManager;
class TransferServer;
class TransferClient;
class QTcpSocket;

class WlanManager : public QObject
{
    Q_OBJECT
public:
    explicit WlanManager(QObject *parent = nullptr);

    //启动WLAN模块：alias=本机显示名，saveDir=接收文件落盘根目录
    void start(const QString &alias, const QString &saveDir);
    void stop();

    //UI确认接受导入请求
    void acceptImport(QTcpSocket *socket, const QStringList &acceptedIds);
    //UI拒绝
    void rejectImport(QTcpSocket *socket);

    //主动向对端发送文件（导出方）
    //localPaths: fileId -> 本地绝对路径
    void sendToPeer(const PeerInfo &peer, const QString &alias,
                    const QList<TransferFileInfo> &files,
                    const QHash<QString, QString> &localPaths);

    QList<PeerInfo> peers() const;

signals:
    void peerListChanged(const QList<PeerInfo> &peers);
    //收到导入请求（UI确认后调acceptImport/rejectImport）
    void importRequested(QTcpSocket *socket, const QString &alias, const QList<TransferFileInfo> &files);
    void fileReceived(const QString &relativePath, const QString &savedPath);
    void progressChanged(const QString &fileId, qint64 done, qint64 total);
    //一批文件接收完毕：给出本次会话创建的顶层目录绝对路径，供主窗口走解析主线
    void importBatchDone(const QStringList &savedTopDirs);
    //发送方相关
    void sendFinished();
    void sendError(const QString &msg);

private slots:
    void onFileReceived(const QString &relativePath, const QString &savedPath);
    void onSessionFinished(const QString &sessionId);
    void onSendClientFinished();
    void onSendClientError(const QString &msg);

private:
    DiscoveryManager *m_discovery;
    TransferServer *m_server;
    QList<TransferClient*> m_clients;

    QString m_saveDir;
    QStringList m_currentBatchPaths;  //当前会话已接收文件的落盘绝对路径（单批汇总用）
};

#endif // WLANMANAGER_H
