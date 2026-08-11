#include "TransferServer.h"
#include "TransferSession.h"
#include "WlanProtocol.h"
#include "logger.h"
#include <QTcpSocket>
#include <QUuid>
#include <QJsonArray>

// ========== 构造 ==========

TransferServer::TransferServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TransferServer::onNewConnection);
}

// ========== 监听 ==========

bool TransferServer::listen(const QString &saveDir)
{
    m_saveDir = saveDir;
    if (!m_server->listen(QHostAddress::Any, WlanProtocol::TRANSFER_PORT)) {
        Logger::instance()->warning("WLAN", QStringLiteral("TCP监听失败: %1").arg(m_server->errorString()));
        return false;
    }
    Logger::instance()->debug("WLAN", QStringLiteral("传输服务端监听端口 %1，落盘目录 %2")
        .arg(WlanProtocol::TRANSFER_PORT).arg(m_saveDir));
    return true;
}

void TransferServer::stop()
{
    m_server->close();
    for (QTcpSocket *sock : m_conns.keys())
        cleanupConnection(sock);
}

quint16 TransferServer::serverPort() const
{
    return m_server->serverPort();
}

// ========== 入站连接 ==========

void TransferServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();

        TransferSession *session = new TransferSession(sock, this);
        session->setSaveDir(m_saveDir);

        ServerConn conn;
        conn.session = session;
        m_conns.insert(sock, conn);

        //用lambda捕获socket，将session信号桥接到server
        connect(session, &TransferSession::frameReceived, this,
                [this, sock](const QJsonObject &header){ handleControlFrame(sock, header); });
        connect(session, &TransferSession::fileReceived, this,
                [this](const QString &rel, const QString &saved){ emit fileReceived(rel, saved); });
        connect(session, &TransferSession::progressChanged, this,
                [this](const QString &fid, qint64 d, qint64 t){ emit progressChanged(fid, d, t); });
        connect(session, &TransferSession::errorOccurred, this,
                [](const QString &msg){ Logger::instance()->warning("WLAN", QStringLiteral("传输错误: %1").arg(msg)); });
        connect(session, &TransferSession::disconnected, this,
                [this, sock](){ cleanupConnection(sock); });

        Logger::instance()->debug("WLAN", QStringLiteral("新入站连接: %1:%2")
            .arg(sock->peerAddress().toString()).arg(sock->peerPort()));
    }
}

// ========== 控制帧处理 ==========

void TransferServer::handleControlFrame(QTcpSocket *socket, const QJsonObject &header)
{
    QString type = header.value(QStringLiteral("type")).toString();
    auto it = m_conns.find(socket);
    if (it == m_conns.end())
        return;
    ServerConn &conn = it.value();

    if (type == QStringLiteral("prepare-import")) {
        conn.alias = header.value(QStringLiteral("alias")).toString();
        conn.files = parseFileList(header.value(QStringLiteral("files")).toArray());
        emit importRequested(socket, conn.alias, conn.files);
    } else if (type == QStringLiteral("complete")) {
        emit sessionFinished(conn.sessionId);
        cleanupConnection(socket);
    } else if (type == QStringLiteral("cancel")) {
        cleanupConnection(socket);
    }
}

QList<TransferFileInfo> TransferServer::parseFileList(const QJsonArray &arr)
{
    QList<TransferFileInfo> files;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        TransferFileInfo f;
        f.id = o.value(QStringLiteral("id")).toString();
        f.relativePath = o.value(QStringLiteral("path")).toString();
        f.size = o.value(QStringLiteral("size")).toVariant().toLongLong();
        files.append(f);
    }
    return files;
}

// ========== 接受/拒绝 ==========

void TransferServer::acceptSession(QTcpSocket *socket, const QStringList &acceptedIds)
{
    auto it = m_conns.find(socket);
    if (it == m_conns.end())
        return;
    ServerConn &conn = it.value();
    //接收方生成sessionId（对应LocalSend由接收方分配）
    conn.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conn.session->sendFrame(WlanProtocol::buildAccept(conn.sessionId, acceptedIds, m_saveDir));
    Logger::instance()->debug("WLAN", QStringLiteral("接受导入 session=%1 文件数=%2")
        .arg(conn.sessionId).arg(acceptedIds.size()));
}

void TransferServer::rejectSession(QTcpSocket *socket)
{
    auto it = m_conns.find(socket);
    if (it != m_conns.end())
        it->session->sendFrame(WlanProtocol::buildCancel(QString()));
    cleanupConnection(socket);
}

// ========== 清理 ==========

void TransferServer::cleanupConnection(QTcpSocket *socket)
{
    auto it = m_conns.find(socket);
    if (it == m_conns.end())
        return;
    it->session->deleteLater();
    m_conns.erase(it);
    socket->deleteLater();
}
