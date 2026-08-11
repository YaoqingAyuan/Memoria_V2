#ifndef TRANSFERCLIENT_H
#define TRANSFERCLIENT_H
//TCP传输客户端：主动向对端发送文件（Memoria作为导出方时运行）
//对应LocalSend的 http/client/v2.rs(发送方)
//
//流程：
//  1. connectToHost 对端 → onConnected 发 prepare-import
//  2. 收 accept → 记sessionId+acceptedIds，按acceptedIds逐文件 sendFileData
//  3. 队列发空(allFilesSent) → 发 complete → emit finished
//  4. 任一步出错 → emit error

#include <QObject>
#include <QTcpSocket>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include "WlanTypes.h"

class TransferSession;

class TransferClient : public QObject
{
    Q_OBJECT
public:
    explicit TransferClient(QObject *parent = nullptr);

    //设置本地文件路径映射（fileId -> 本地绝对路径），发送时按此读取文件
    void setLocalFiles(const QHash<QString, QString> &fileIdToLocalPath);

    //连接对端并发起导入请求
    void sendImportRequest(const QHostAddress &peerIp, quint16 peerPort,
                           const QString &alias,
                           const QList<TransferFileInfo> &files);

    //取消发送：发cancel帧并断开
    void cancel();

signals:
    void accepted(const QString &sessionId, const QStringList &acceptedIds);
    void fileSent(const QString &fileId);
    void progressChanged(const QString &fileId, qint64 done, qint64 total);
    void finished();
    void error(const QString &msg);

private slots:
    void onConnected();
    void onFrameReceived(const QJsonObject &header);
    void onFileSent(const QString &fileId);
    void onAllFilesSent();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    TransferSession *m_session;

    QString m_alias;
    QList<TransferFileInfo> m_files;          //待发送文件清单
    QHash<QString, QString> m_localPaths;     //fileId -> 本地绝对路径
    QString m_sessionId;
    QStringList m_acceptedIds;
    bool m_finished = false;                   //已发finished，避免断开时重复报错
};

#endif // TRANSFERCLIENT_H
