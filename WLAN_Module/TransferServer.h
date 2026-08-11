#ifndef TRANSFERSERVER_H
#define TRANSFERSERVER_H
//TCP传输服务端：被动接收导入文件（Memoria作为导入方时运行）
//对应LocalSend的 http/server/v2.rs(接收端端点)
//
//流程：
//  1. 监听 TRANSFER_PORT，为每条入站连接创建一个 TransferSession
//  2. 收到 prepare-import → emit importRequested（UI确认）
//  3. UI确认后调 acceptSession → 回 accept 帧（含sessionId）
//  4. 发送方开始发 file-data，session流式写盘 → emit fileReceived/progressChanged
//  5. 收到 complete → emit sessionFinished
//
//单会话模型：参考LocalSend，当前实现允许并发连接但WlanManager按单批汇总

#include <QObject>
#include <QTcpServer>
#include <QHash>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "WlanTypes.h"

class QTcpSocket;
class TransferSession;

class TransferServer : public QObject
{
    Q_OBJECT
public:
    explicit TransferServer(QObject *parent = nullptr);

    //开始监听；saveDir为接收文件落盘根目录
    bool listen(const QString &saveDir);
    void stop();
    quint16 serverPort() const;

    //UI确认接受后调用：生成sessionId并发送accept帧
    void acceptSession(QTcpSocket *socket, const QStringList &acceptedIds);
    //UI拒绝：发送cancel帧并关闭连接
    void rejectSession(QTcpSocket *socket);

signals:
    //收到导入请求（需UI确认后调acceptSession/rejectSession）
    void importRequested(QTcpSocket *socket, const QString &peerAlias, const QList<TransferFileInfo> &files);
    void fileReceived(const QString &relativePath, const QString &savedPath);
    void progressChanged(const QString &fileId, qint64 done, qint64 total);
    void sessionFinished(const QString &sessionId);

private slots:
    void onNewConnection();

private:
    //每条连接的上下文
    struct ServerConn {
        TransferSession *session = nullptr;
        QString alias;
        QList<TransferFileInfo> files;
        QString sessionId;  //空表示尚未接受
    };

    QTcpServer *m_server;
    QString m_saveDir;
    QHash<QTcpSocket*, ServerConn> m_conns;

    void handleControlFrame(QTcpSocket *socket, const QJsonObject &header);
    void cleanupConnection(QTcpSocket *socket);

    //解析 prepare-import 中的文件清单
    static QList<TransferFileInfo> parseFileList(const class QJsonArray &arr);
};

#endif // TRANSFERSERVER_H
