#ifndef TRANSFERSESSION_H
#define TRANSFERSESSION_H
//单TCP连接的状态机：流式收发 + 进度
//对应LocalSend的 session.rs(SessionStateV2状态机) + UploadGuard(连接断开时清理未完成文件)
//
//收发共用：
//  - 接收方(TransferServer)：每条入站连接创建一个session，setSaveDir后自动流式写盘
//  - 发送方(TransferClient)：在出站socket上创建session，调用sendFileData流式读盘发送
//
//接收状态机：ReadingHeader(读帧头) ↔ ReadingFileData(分块写盘)，靠"size"字段驱动切换
//发送状态机：发送队列 → 逐文件(写帧头 + 分块读盘write) → bytesWritten回触发继续

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QQueue>
#include <QString>
#include <QJsonObject>
#include "WlanTypes.h"

class QFile;

class TransferSession : public QObject
{
    Q_OBJECT
public:
    explicit TransferSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~TransferSession();

    //接收方设置落盘根目录；收到的文件按relativePath写入其下
    void setSaveDir(const QString &dir);

    //发送一个控制帧（非文件数据）
    void sendFrame(const QJsonObject &header);

    //发送方：入队一个待发文件，若当前空闲则立即开始，否则排队串行发送
    void sendFileData(const QString &sessionId, const TransferFileInfo &info, const QString &localPath);

    //发送方：清空发送队列并中止当前文件读取
    void cancelSending();

signals:
    //收到一个控制帧（prepare-import/accept/complete/cancel）
    void frameReceived(const QJsonObject &header);
    //接收方：一个文件接收完毕
    void fileReceived(const QString &relativePath, const QString &savedPath);
    //发送方：一个文件发送完毕
    void fileSent(const QString &fileId);
    //发送方：发送队列全部完成
    void allFilesSent();
    //进度（收或发）
    void progressChanged(const QString &fileId, qint64 done, qint64 total);
    //错误
    void errorOccurred(const QString &msg);
    //底层连接断开
    void disconnected();

private slots:
    void onReadyRead();             //接收状态机：头/数据交替读取
    void onBytesWritten(qint64 bytes);  //发送状态机：触发下一块
    void onDisconnected();          //清理未完成文件

private:
    QTcpSocket *m_socket;
    QString m_saveDir;

    // ===== 接收状态 =====
    enum class Phase { ReadingHeader, ReadingFileData };
    Phase m_phase = Phase::ReadingHeader;
    QByteArray m_rxBuffer;          //接收缓冲区（累积未消费字节）
    QString m_currentFileId;        //当前接收/发送中的文件id
    QString m_currentRelativePath;
    QString m_currentSavedPath;
    qint64 m_currentTotal = 0;
    qint64 m_currentReceived = 0;
    QFile *m_rxFile = nullptr;      //正在写入的接收文件

    // ===== 发送状态 =====
    struct SendTask {
        QString sessionId;
        TransferFileInfo info;
        QString localPath;
    };
    QQueue<SendTask> m_sendQueue;
    QFile *m_txFile = nullptr;      //正在读取的发送文件
    qint64 m_txTotal = 0;
    qint64 m_txSent = 0;
    bool m_txFileDone = false;      //文件已读完（socket可能仍在flush）
    bool m_sending = false;         //是否正在发送某个文件

    void handleHeader(const QJsonObject &header);   //分发收到的帧头
    void startReceivingFile(const QJsonObject &header); //进入ReadingFileData
    void startNextSend();           //取队列下一文件开始发送
    void pumpSend();                //继续读盘写入socket（受水位控制）
};

#endif // TRANSFERSESSION_H
