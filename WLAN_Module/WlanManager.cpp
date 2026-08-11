#include "WlanManager.h"
#include "DiscoveryManager.h"
#include "TransferServer.h"
#include "TransferClient.h"
#include "WlanProtocol.h"
#include "Core/logger.h"
#include <QTcpSocket>
#include <QDir>
#include <QSet>
#include <QFileInfo>

// ========== 构造 ==========

WlanManager::WlanManager(QObject *parent)
    : QObject(parent),
      m_discovery(new DiscoveryManager(this)),
      m_server(new TransferServer(this))
{
    //发现层 → 对外
    connect(m_discovery, &DiscoveryManager::peerListChanged, this, &WlanManager::peerListChanged);

    //接收侧 → 对外
    connect(m_server, &TransferServer::importRequested, this, &WlanManager::importRequested);
    connect(m_server, &TransferServer::fileReceived, this, &WlanManager::onFileReceived);
    connect(m_server, &TransferServer::progressChanged, this, &WlanManager::progressChanged);
    connect(m_server, &TransferServer::sessionFinished, this, &WlanManager::onSessionFinished);
}

// ========== 启动/停止 ==========

void WlanManager::start(const QString &alias, const QString &saveDir)
{
    m_saveDir = saveDir;
    QDir().mkpath(saveDir);

    m_discovery->start(alias, WlanProtocol::TRANSFER_PORT);
    m_server->listen(saveDir);

    Logger::instance()->debug("WLAN", QStringLiteral("WLAN模块已启动 alias=%1 落盘=%2").arg(alias, saveDir));
}

void WlanManager::stop()
{
    m_discovery->stop();
    m_server->stop();
    for (TransferClient *c : m_clients) {
        c->cancel();
        c->deleteLater();
    }
    m_clients.clear();
    m_currentBatchPaths.clear();
}

// ========== 接收侧：接受/拒绝 ==========

void WlanManager::acceptImport(QTcpSocket *socket, const QStringList &acceptedIds)
{
    m_server->acceptSession(socket, acceptedIds);
}

void WlanManager::rejectImport(QTcpSocket *socket)
{
    m_server->rejectSession(socket);
}

// ========== 发送侧 ==========

void WlanManager::sendToPeer(const PeerInfo &peer, const QString &alias,
                             const QList<TransferFileInfo> &files,
                             const QHash<QString, QString> &localPaths)
{
    TransferClient *client = new TransferClient(this);
    m_clients.append(client);

    connect(client, &TransferClient::progressChanged, this, &WlanManager::progressChanged);
    connect(client, &TransferClient::finished, this, &WlanManager::onSendClientFinished);
    connect(client, &TransferClient::error, this, &WlanManager::onSendClientError);

    client->setLocalFiles(localPaths);
    client->sendImportRequest(peer.ip, peer.port, alias, files);
}

QList<PeerInfo> WlanManager::peers() const
{
    return m_discovery->peers();
}

// ========== 接收侧：汇总批次 ==========

void WlanManager::onFileReceived(const QString &relativePath, const QString &savedPath)
{
    //单批模型：累积本次会话所有落盘文件，会话结束时汇总顶层目录
    m_currentBatchPaths.append(savedPath);
    emit fileReceived(relativePath, savedPath);
}

void WlanManager::onSessionFinished(const QString &sessionId)
{
    Q_UNUSED(sessionId);
    //将落盘文件汇总为顶层目录：如 saveDir/视频A/Video.m4s → saveDir/视频A
    QSet<QString> topDirs;
    for (const QString &p : m_currentBatchPaths) {
        QString rel = QDir(m_saveDir).relativeFilePath(p);
        QString top = rel.section(QLatin1Char('/'), 0, 0);
        if (!top.isEmpty() && top != rel)
            topDirs.insert(QDir(m_saveDir).absoluteFilePath(top));
        else
            topDirs.insert(p);  //无子目录结构，直接给文件路径
    }

    Logger::instance()->debug("WLAN", QStringLiteral("导入批次完成，顶层目录数=%1").arg(topDirs.size()));
    emit importBatchDone(topDirs.values());
    m_currentBatchPaths.clear();
}

// ========== 发送侧：客户端清理 ==========

void WlanManager::onSendClientFinished()
{
    TransferClient *client = qobject_cast<TransferClient*>(sender());
    if (client) {
        m_clients.removeOne(client);
        client->deleteLater();
    }
    emit sendFinished();
}

void WlanManager::onSendClientError(const QString &msg)
{
    Logger::instance()->warning("WLAN", QStringLiteral("发送错误: %1").arg(msg));
    emit sendError(msg);
}
