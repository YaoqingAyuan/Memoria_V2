#include "TransferClient.h"
#include "TransferSession.h"
#include "WlanProtocol.h"
#include "logger.h"

// ========== 构造 ==========

TransferClient::TransferClient(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_session(new TransferSession(m_socket, this))
{
    connect(m_socket, &QTcpSocket::connected, this, &TransferClient::onConnected);
    connect(m_session, &TransferSession::frameReceived, this, &TransferClient::onFrameReceived);
    connect(m_session, &TransferSession::fileSent, this, &TransferClient::onFileSent);
    connect(m_session, &TransferSession::allFilesSent, this, &TransferClient::onAllFilesSent);
    connect(m_session, &TransferSession::progressChanged, this,
            [this](const QString &fid, qint64 d, qint64 t){ emit progressChanged(fid, d, t); });
    connect(m_session, &TransferSession::errorOccurred, this,
            [this](const QString &msg){ emit error(msg); });
    connect(m_session, &TransferSession::disconnected, this, &TransferClient::onDisconnected);
}

// ========== 公共接口 ==========

void TransferClient::setLocalFiles(const QHash<QString, QString> &fileIdToLocalPath)
{
    m_localPaths = fileIdToLocalPath;
}

void TransferClient::sendImportRequest(const QHostAddress &peerIp, quint16 peerPort,
                                       const QString &alias,
                                       const QList<TransferFileInfo> &files)
{
    m_alias = alias;
    m_files = files;
    m_finished = false;
    m_socket->connectToHost(peerIp, peerPort);
}

void TransferClient::cancel()
{
    m_session->sendFrame(WlanProtocol::buildCancel(m_sessionId));
    m_session->cancelSending();
    m_socket->abort();
}

// ========== 内部流程 ==========

void TransferClient::onConnected()
{
    //发送prepare-import，等待对端accept
    m_session->sendFrame(WlanProtocol::buildPrepareImport(m_alias, m_files));
}

void TransferClient::onFrameReceived(const QJsonObject &header)
{
    QString type = header.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("accept")) {
        m_sessionId = header.value(QStringLiteral("sessionId")).toString();
        QJsonArray arr = header.value(QStringLiteral("accepted")).toArray();
        m_acceptedIds.clear();
        for (const QJsonValue &v : arr)
            m_acceptedIds.append(v.toString());
        emit accepted(m_sessionId, m_acceptedIds);

        //无文件被接受 → 直接结束
        if (m_acceptedIds.isEmpty()) {
            m_session->sendFrame(WlanProtocol::buildComplete(m_sessionId));
            m_finished = true;
            emit finished();
            m_socket->disconnectFromHost();
            return;
        }

        //按acceptedIds顺序入队发送（session内部串行处理）
        for (const QString &id : m_acceptedIds) {
            for (const TransferFileInfo &f : m_files) {
                if (f.id == id) {
                    QString local = m_localPaths.value(id);
                    if (local.isEmpty()) {
                        emit error(QStringLiteral("缺少本地文件映射: %1").arg(id));
                        continue;
                    }
                    m_session->sendFileData(m_sessionId, f, local);
                    break;
                }
            }
        }
    } else if (type == QStringLiteral("cancel")) {
        emit error(QStringLiteral("对端取消了导入"));
        m_socket->abort();
    }
}

void TransferClient::onFileSent(const QString &fileId)
{
    emit fileSent(fileId);
}

void TransferClient::onAllFilesSent()
{
    //全部文件发完 → 发complete → 结束
    m_session->sendFrame(WlanProtocol::buildComplete(m_sessionId));
    m_finished = true;
    emit finished();
    m_socket->disconnectFromHost();
}

void TransferClient::onDisconnected()
{
    //非正常结束的断开视为错误
    if (!m_finished)
        emit error(QStringLiteral("连接异常断开"));
}
