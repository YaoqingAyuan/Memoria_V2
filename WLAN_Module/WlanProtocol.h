#ifndef WLANPROTOCOL_H
#define WLANPROTOCOL_H
//WLAN自定义传输协议：帧编解码 + JSON消息构造 + 协议常量
//
//帧格式（收发一致）:
//   [4字节大端JSON长度][JSON头]
//   file-data 帧的 JSON 头含 "size" 字段，其后紧跟 size 字节原始文件数据
//   （原始数据由 TransferSession 状态机流式处理，不进入JSON解码）
//其余帧无附加字节。
//
//对应LocalSend的 http/dto_v2.rs(JSON DTO定义) + url.rs(端口/路由常量)，
//用自定义二进制帧替代其 HTTP REST 路由（封闭生态无需REST兼容）。

#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include "WlanTypes.h"

class WlanProtocol
{
public:
    WlanProtocol() = delete;  //纯静态工具类，禁止实例化

    // ========== 协议常量 ==========
    static const QHostAddress MULTICAST_GROUP;   //224.0.0.167
    static const quint16 DISCOVERY_PORT = 53317; //UDP多播端口（与LocalSend一致）
    static const quint16 TRANSFER_PORT  = 53318; //TCP传输端口（独立于发现端口）

    // ========== 帧编解码 ==========
    //编码一帧控制消息：4字节大端JSON长度 + JSON头
    static QByteArray encodeFrame(const QJsonObject &header);

    //从缓冲区头部尝试取出一帧JSON头；
    //成功则从buffer消费对应字节、填充header并返回true；
    //不完整则保持buffer不变、返回false。
    static bool tryTakeFrame(QByteArray &buffer, QJsonObject &header);

    // ========== JSON消息构造 ==========
    //UDP多播通告（发现层用）
    static QJsonObject buildAnnounce(const QString &alias, const QString &fingerprint, quint16 port);
    //请求导入（发送方→接收方），携带文件清单
    static QJsonObject buildPrepareImport(const QString &alias, const QList<TransferFileInfo> &files);
    //接受导入（接收方→发送方），携带sessionId + 接受的fileId列表 + 落盘目录
    static QJsonObject buildAccept(const QString &sessionId, const QStringList &acceptedIds, const QString &saveDir);
    //文件数据帧头（发送方→接收方），紧跟size字节原始数据
    static QJsonObject buildFileData(const QString &sessionId, const TransferFileInfo &f);
    //全部完成（发送方→接收方）
    static QJsonObject buildComplete(const QString &sessionId);
    //取消会话（任一方可发）
    static QJsonObject buildCancel(const QString &sessionId);
};

#endif // WLANPROTOCOL_H
