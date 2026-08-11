#include "TransferSession.h"
#include "WlanProtocol.h"
#include "utils.h"
#include "logger.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

namespace {
    const qint64 SEND_CHUNK = 65536;        //64KB 读盘块
    const qint64 SEND_WATERMARK = 256 * 1024; //socket缓冲水位，低于此值才继续喂
}

//将相对路径清理为安全的落盘子路径：过滤 . / .. 防目录穿越，逐段用sanitizeFileName
static QString sanitizeRelativePath(const QString &rel)
{
    QStringList parts = rel.split('/');
    QStringList safe;
    for (const QString &p : parts) {
        if (p.isEmpty() || p == QStringLiteral(".") || p == QStringLiteral(".."))
            continue;  //防目录穿越
        safe.append(sanitizeFileName(p));
    }
    return safe.join('/');
}

// ========== 构造/析构 ==========

TransferSession::TransferSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &TransferSession::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TransferSession::onBytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, &TransferSession::onDisconnected);
}

TransferSession::~TransferSession()
{
    //清理未关闭的文件句柄（socket由上层负责释放）
    if (m_rxFile) {
        m_rxFile->close();
        delete m_rxFile;
    }
    if (m_txFile) {
        m_txFile->close();
        delete m_txFile;
    }
}

void TransferSession::setSaveDir(const QString &dir)
{
    m_saveDir = dir;
}

// ========== 发送：控制帧 / 文件数据 ==========

void TransferSession::sendFrame(const QJsonObject &header)
{
    m_socket->write(WlanProtocol::encodeFrame(header));
}

void TransferSession::sendFileData(const QString &sessionId, const TransferFileInfo &info, const QString &localPath)
{
    m_sendQueue.enqueue({sessionId, info, localPath});
    if (!m_sending)
        startNextSend();
}

void TransferSession::cancelSending()
{
    m_sendQueue.clear();
    if (m_txFile) {
        m_txFile->close();
        delete m_txFile;
        m_txFile = nullptr;
    }
    m_sending = false;
}

void TransferSession::startNextSend()
{
    if (m_sendQueue.isEmpty()) {
        m_sending = false;
        emit allFilesSent();
        return;
    }

    SendTask task = m_sendQueue.dequeue();
    m_currentFileId = task.info.id;
    m_txTotal = task.info.size;
    m_txSent = 0;
    m_txFileDone = false;

    m_txFile = new QFile(task.localPath, this);
    if (!m_txFile->open(QIODevice::ReadOnly)) {
        emit errorOccurred(QStringLiteral("无法打开发送文件: %1").arg(task.localPath));
        delete m_txFile;
        m_txFile = nullptr;
        startNextSend();  //跳过，继续下一个
        return;
    }

    m_sending = true;
    //先写file-data帧头，随后流式发送文件内容
    sendFrame(WlanProtocol::buildFileData(task.sessionId, task.info));
    pumpSend();
}

void TransferSession::pumpSend()
{
    if (!m_txFile || !m_txFile->isOpen())
        return;

    //水位控制：socket缓冲过高时停手，等bytesWritten回触发再继续，避免内存堆积
    while (m_socket->bytesToWrite() < SEND_WATERMARK) {
        QByteArray chunk = m_txFile->read(SEND_CHUNK);
        if (chunk.isEmpty()) {
            m_txFile->close();
            m_txFileDone = true;
            break;
        }
        m_txSent += chunk.size();
        m_socket->write(chunk);
        emit progressChanged(m_currentFileId, m_txSent, m_txTotal);
    }
}

void TransferSession::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    if (!m_sending)
        return;

    //当前文件已读完且socket缓冲已flush → 本文件发送完毕
    if (m_txFileDone && m_socket->bytesToWrite() == 0) {
        if (m_txFile) {
            m_txFile->deleteLater();
            m_txFile = nullptr;
        }
        m_sending = false;
        emit fileSent(m_currentFileId);
        startNextSend();
    } else if (m_txFile && m_txFile->isOpen()) {
        pumpSend();
    }
}

// ========== 接收：状态机 ==========

void TransferSession::onReadyRead()
{
    m_rxBuffer.append(m_socket->readAll());

    //循环消费缓冲区，直到不足以再取一帧/一块
    bool progress = true;
    while (progress) {
        progress = false;

        if (m_phase == Phase::ReadingHeader) {
            QJsonObject header;
            if (WlanProtocol::tryTakeFrame(m_rxBuffer, header)) {
                progress = true;
                handleHeader(header);
            }
        } else {  //ReadingFileData
            if (!m_rxBuffer.isEmpty() && m_currentReceived < m_currentTotal) {
                qint64 want = m_currentTotal - m_currentReceived;
                qint64 take = qMin(static_cast<qint64>(m_rxBuffer.size()), want);
                QByteArray chunk = m_rxBuffer.left(static_cast<int>(take));
                m_rxBuffer.remove(0, static_cast<int>(take));
                if (m_rxFile)
                    m_rxFile->write(chunk);
                m_currentReceived += take;
                emit progressChanged(m_currentFileId, m_currentReceived, m_currentTotal);

                if (m_currentReceived >= m_currentTotal) {
                    //文件接收完毕
                    QString rel = m_currentRelativePath;
                    QString saved = m_currentSavedPath;
                    if (m_rxFile) {
                        m_rxFile->close();
                        m_rxFile->deleteLater();
                        m_rxFile = nullptr;
                    }
                    m_phase = Phase::ReadingHeader;
                    emit fileReceived(rel, saved);
                }
                progress = true;
            }
        }
    }
}

void TransferSession::handleHeader(const QJsonObject &header)
{
    QString type = header.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("file-data")) {
        startReceivingFile(header);
    } else {
        //控制帧交给上层（TransferServer/TransferClient）
        emit frameReceived(header);
    }
}

void TransferSession::startReceivingFile(const QJsonObject &header)
{
    m_currentFileId = header.value(QStringLiteral("fileId")).toString();
    m_currentRelativePath = header.value(QStringLiteral("path")).toString();
    m_currentTotal = header.value(QStringLiteral("size")).toVariant().toLongLong();
    m_currentReceived = 0;

    //构造落盘路径并创建父目录
    QString safePath = sanitizeRelativePath(m_currentRelativePath);
    m_currentSavedPath = QDir(m_saveDir).absoluteFilePath(safePath);
    QDir().mkpath(QFileInfo(m_currentSavedPath).absolutePath());

    m_rxFile = new QFile(m_currentSavedPath, this);
    if (!m_rxFile->open(QIODevice::WriteOnly)) {
        Logger::instance()->warning("WLAN", QStringLiteral("无法创建接收文件: %1").arg(m_currentSavedPath));
        delete m_rxFile;
        m_rxFile = nullptr;  //进入丢弃模式：仍计数以保持状态机同步
    }

    m_phase = Phase::ReadingFileData;
}

void TransferSession::onDisconnected()
{
    //等价LocalSend UploadGuard的Drop语义：清理未完成文件
    if (m_rxFile) {
        QString incomplete = m_rxFile->fileName();
        m_rxFile->close();
        m_rxFile->deleteLater();
        m_rxFile = nullptr;
        QFile::remove(incomplete);  //删除未完成的接收文件
    }
    if (m_txFile) {
        m_txFile->close();
        m_txFile->deleteLater();
        m_txFile = nullptr;
    }
    m_sending = false;
    m_sendQueue.clear();
    emit disconnected();
}
