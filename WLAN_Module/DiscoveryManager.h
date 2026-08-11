#ifndef DISCOVERYMANAGER_H
#define DISCOVERYMANAGER_H
//UDP多播设备发现层
//对应LocalSend的 multicast/mod.rs(多播收发) + discovery/mod.rs(发现编排)
//
//职责：
//  1. 周期性向 224.0.0.167:53317 多播自身通告
//  2. 监听对端通告，解析得到 PeerInfo，fingerprint 自过滤（避免收到自己）
//  3. 维护对端列表，超时未收到通告则判定离线
//对应关系：QUdpSocket::bind+joinMulticastGroup = Rust SO_REUSEPORT+IP_ADD_MEMBERSHIP

#include <QObject>
#include <QUdpSocket>
#include <QHash>
#include <QList>
#include <QTimer>
#include <QString>
#include "WlanTypes.h"

class DiscoveryManager : public QObject
{
    Q_OBJECT
public:
    explicit DiscoveryManager(QObject *parent = nullptr);

    //启动发现：绑定端口 + 加入多播组 + 开始定时通告
    //alias为本机显示名，port为本机TCP传输端口（通告给对端，便于其回连）
    void start(const QString &alias, quint16 port);

    //停止发现，离开多播组，清空对端列表
    void stop();

    //当前已发现的对端列表
    QList<PeerInfo> peers() const;

    //本机fingerprint（设备唯一标识）
    QString fingerprint() const { return m_fingerprint; }

signals:
    //发现新对端（首次）
    void peerDiscovered(const PeerInfo &peer);
    //对端超时离线
    void peerLost(const QString &fingerprint);
    //对端列表整体变化（增/删时发出，便于UI整体刷新）
    void peerListChanged(const QList<PeerInfo> &peers);

private slots:
    void onReadyRead();         //解析收到的UDP数据报
    void onAnnounceTimeout();   //定时发送通告
    void onSweepTimeout();      //定时清理超时对端

private:
    QUdpSocket *m_socket;
    QString m_alias;
    QString m_fingerprint;
    quint16 m_transferPort = 0;

    QHash<QString, PeerInfo> m_peers;       //fingerprint -> peer
    QHash<QString, qint64> m_lastSeen;      //fingerprint -> 上次收到通告的时间戳(ms)

    QTimer *m_announceTimer;   //通告定时器
    QTimer *m_sweepTimer;      //清理定时器

    void emitListChanged();
};

#endif // DISCOVERYMANAGER_H
