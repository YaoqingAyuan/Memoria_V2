#include "DiscoveryManager.h"
#include "WlanProtocol.h"
#include "logger.h"
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDateTime>
#include <QSysInfo>
#include <QCryptographicHash>

namespace {
    const int ANNOUNCE_INTERVAL_MS = 5000;   //5秒通告一次
    const int SWEEP_INTERVAL_MS = 15000;     //15秒清理一次
    const int PEER_TIMEOUT_MS = 20000;       //20秒未收到通告视为离线
}

//生成本机fingerprint：机器唯一标识的SHA-256（大写HEX）
//无证书环境用硬件标识替代LocalSend的证书指纹
static QString generateFingerprint()
{
    QByteArray machineId = QSysInfo::machineUniqueId();
    if (machineId.isEmpty())
        machineId = QSysInfo::machineHostName().toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(machineId, QCryptographicHash::Sha256).toHex()).toUpper();
}

// ========== 构造/析构 ==========

DiscoveryManager::DiscoveryManager(QObject *parent)
    : QObject(parent),
      m_socket(new QUdpSocket(this)),
      m_announceTimer(new QTimer(this)),
      m_sweepTimer(new QTimer(this))
{
    m_fingerprint = generateFingerprint();

    connect(m_socket, &QUdpSocket::readyRead, this, &DiscoveryManager::onReadyRead);
    connect(m_announceTimer, &QTimer::timeout, this, &DiscoveryManager::onAnnounceTimeout);
    connect(m_sweepTimer, &QTimer::timeout, this, &DiscoveryManager::onSweepTimeout);
}

// ========== 启动/停止 ==========

void DiscoveryManager::start(const QString &alias, quint16 port)
{
    m_alias = alias;
    m_transferPort = port;

    //绑定：ShareAddress允许同机多实例(开发调试)，ReuseAddressHint避免端口占用报错
    if (!m_socket->bind(QHostAddress::AnyIPv4, WlanProtocol::DISCOVERY_PORT,
                        QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        Logger::instance()->warning("WLAN", QStringLiteral("UDP绑定失败: %1").arg(m_socket->errorString()));
        return;
    }

    //加入多播组：逐个非环回IPv4接口加入（对应LocalSend每接口绑一个socket的策略）
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        if (!m_socket->joinMulticastGroup(WlanProtocol::MULTICAST_GROUP, iface)) {
            Logger::instance()->debug("WLAN", QStringLiteral("加入多播组失败(接口%1): %2")
                .arg(iface.humanReadableName(), m_socket->errorString()));
        }
    }

    //TTL=1 仅本地子网；多播回环开启，靠fingerprint自过滤自身消息
    m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
    m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

    m_announceTimer->start(ANNOUNCE_INTERVAL_MS);
    m_sweepTimer->start(SWEEP_INTERVAL_MS);
    onAnnounceTimeout();  //立即通告一次

    Logger::instance()->debug("WLAN", QStringLiteral("发现层已启动 alias=%1 fp=%2").arg(m_alias, m_fingerprint));
}

void DiscoveryManager::stop()
{
    m_announceTimer->stop();
    m_sweepTimer->stop();
    m_socket->leaveMulticastGroup(WlanProtocol::MULTICAST_GROUP);
    m_socket->abort();
    m_peers.clear();
    m_lastSeen.clear();
}

QList<PeerInfo> DiscoveryManager::peers() const
{
    return m_peers.values();
}

// ========== 定时通告 ==========

void DiscoveryManager::onAnnounceTimeout()
{
    QJsonObject msg = WlanProtocol::buildAnnounce(m_alias, m_fingerprint, m_transferPort);
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->writeDatagram(data, WlanProtocol::MULTICAST_GROUP, WlanProtocol::DISCOVERY_PORT);
}

// ========== 接收通告 ==========

void DiscoveryManager::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress senderIp;
        quint16 senderPort = 0;
        m_socket->readDatagram(data.data(), data.size(), &senderIp, &senderPort);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError)
            continue;
        QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("announce"))
            continue;

        QString fp = obj.value(QStringLiteral("fingerprint")).toString();
        if (fp.isEmpty() || fp == m_fingerprint)
            continue;  //过滤自身

        PeerInfo peer;
        peer.alias = obj.value(QStringLiteral("alias")).toString();
        peer.fingerprint = fp;
        peer.ip = senderIp;  //数据报发送方IP即对端可达地址
        peer.port = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt());

        m_lastSeen[fp] = QDateTime::currentMSecsSinceEpoch();
        if (!m_peers.contains(fp)) {
            m_peers.insert(fp, peer);
            emit peerDiscovered(peer);
            emitListChanged();
            Logger::instance()->debug("WLAN", QStringLiteral("发现对端: %1 @ %2:%3")
                .arg(peer.alias, peer.ip.toString()).arg(peer.port));
        } else {
            m_peers[fp] = peer;  //更新（别名/IP可能变化）
        }
    }
}

// ========== 超时清理 ==========

void DiscoveryManager::onSweepTimeout()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> dead;
    for (auto it = m_lastSeen.constBegin(); it != m_lastSeen.constEnd(); ++it) {
        if (now - it.value() > PEER_TIMEOUT_MS)
            dead.append(it.key());
    }
    for (const QString &fp : dead) {
        m_peers.remove(fp);
        m_lastSeen.remove(fp);
        emit peerLost(fp);
        Logger::instance()->debug("WLAN", QStringLiteral("对端超时离线: %1").arg(fp));
    }
    if (!dead.isEmpty())
        emitListChanged();
}

void DiscoveryManager::emitListChanged()
{
    emit peerListChanged(peers());
}
