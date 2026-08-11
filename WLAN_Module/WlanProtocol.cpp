#include "WlanProtocol.h"
#include <QJsonDocument>
#include <QJsonParseError>

//协议常量定义（整型常量在头文件内联初始化，QHostAddress需在此定义）
const QHostAddress WlanProtocol::MULTICAST_GROUP("224.0.0.167");

// ========== 帧编解码 ==========

QByteArray WlanProtocol::encodeFrame(const QJsonObject &header)
{
    QByteArray json = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray frame;
    frame.reserve(4 + json.size());

    //4字节大端序写入JSON长度
    quint32 len = static_cast<quint32>(json.size());
    frame.append(static_cast<char>((len >> 24) & 0xFF));
    frame.append(static_cast<char>((len >> 16) & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(json);
    return frame;
}

bool WlanProtocol::tryTakeFrame(QByteArray &buffer, QJsonObject &header)
{
    if (buffer.size() < 4)
        return false;

    //读取4字节大端序JSON长度
    quint32 len = (static_cast<quint8>(buffer[0]) << 24)
                | (static_cast<quint8>(buffer[1]) << 16)
                | (static_cast<quint8>(buffer[2]) << 8)
                |  static_cast<quint8>(buffer[3]);

    //整帧尚未到齐
    if (buffer.size() < 4 + static_cast<int>(len))
        return false;

    QByteArray json = buffer.mid(4, static_cast<int>(len));
    buffer.remove(0, 4 + static_cast<int>(len));

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    header = doc.object();
    return true;
}

// ========== JSON消息构造 ==========

QJsonObject WlanProtocol::buildAnnounce(const QString &alias, const QString &fingerprint, quint16 port)
{
    QJsonObject obj;
    obj["type"] = "announce";
    obj["alias"] = alias;
    obj["fingerprint"] = fingerprint;
    obj["port"] = static_cast<int>(port);
    return obj;
}

QJsonObject WlanProtocol::buildPrepareImport(const QString &alias, const QList<TransferFileInfo> &files)
{
    QJsonObject obj;
    obj["type"] = "prepare-import";
    obj["alias"] = alias;

    QJsonArray arr;
    for (const TransferFileInfo &f : files) {
        QJsonObject fo;
        fo["id"] = f.id;
        fo["path"] = f.relativePath;
        fo["size"] = static_cast<qint64>(f.size);
        arr.append(fo);
    }
    obj["files"] = arr;
    return obj;
}

QJsonObject WlanProtocol::buildAccept(const QString &sessionId, const QStringList &acceptedIds, const QString &saveDir)
{
    QJsonObject obj;
    obj["type"] = "accept";
    obj["sessionId"] = sessionId;

    QJsonArray arr;
    for (const QString &id : acceptedIds)
        arr.append(id);
    obj["accepted"] = arr;
    obj["saveDir"] = saveDir;
    return obj;
}

QJsonObject WlanProtocol::buildFileData(const QString &sessionId, const TransferFileInfo &f)
{
    QJsonObject obj;
    obj["type"] = "file-data";
    obj["sessionId"] = sessionId;
    obj["fileId"] = f.id;
    obj["path"] = f.relativePath;
    obj["size"] = static_cast<qint64>(f.size);
    return obj;
}

QJsonObject WlanProtocol::buildComplete(const QString &sessionId)
{
    QJsonObject obj;
    obj["type"] = "complete";
    obj["sessionId"] = sessionId;
    return obj;
}

QJsonObject WlanProtocol::buildCancel(const QString &sessionId)
{
    QJsonObject obj;
    obj["type"] = "cancel";
    obj["sessionId"] = sessionId;
    return obj;
}
