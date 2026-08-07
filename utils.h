#ifndef UTILS_H
#define UTILS_H
//工具类（简单工具，无需封装）

#include <QString>
#include <QFile>
#include <QCryptographicHash>
#include <QByteArrayView>
#include <QRegularExpression>

//计算文件内容的MD5哈希值，返回十六进制字符串的前prefixLength位
//分块读取以支持大文件(m4s可达数百MB)，返回空字符串表示文件无法打开
inline QString fileHashPrefix(const QString &filePath, int prefixLength = 8)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Md5);
    const qint64 bufferSize = 1024 * 1024;  //1MB分块读取
    QByteArray buffer;
    buffer.resize(static_cast<int>(bufferSize));

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        if (bytesRead <= 0)
            break;
        hash.addData(QByteArrayView(buffer.constData(), static_cast<qsizetype>(bytesRead)));
    }
    file.close();

    //MD5结果为16字节，转十六进制得32字符，取前prefixLength位
    QString hex = QString::fromLatin1(hash.result().toHex());
    return hex.left(prefixLength);
}

//清理字符串使其可作为安全的Windows文件名
//处理顺序: 控制字符 → 非法字符(\ / : * ? " < > |) → 末尾空格/点号 → Windows保留名 → 空标题兜底
inline QString sanitizeFileName(const QString &name, const QString &fallback = QStringLiteral("untitled"))
{
    QString safe = name;

    //1. 移除控制字符(\x00-\x1F)
    safe.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F]")));

    //2. 替换Windows文件名非法字符: \ / : * ? " < > |
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));

    //3. 去除末尾空格和点号(Windows不允许文件名以空格或点号结尾)
    safe.replace(QRegularExpression(QStringLiteral("[ .]+$")), QString());

    //4. 处理Windows保留设备名(CON/PRN/AUX/NUL/COM1-9/LPT1-9)，追加下划线避免冲突
    static const QRegularExpression reservedName(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
        QRegularExpression::CaseInsensitiveOption);
    if (reservedName.match(safe.trimmed()).hasMatch())
        safe += QStringLiteral("_");

    //5. 空标题兜底
    if (safe.trimmed().isEmpty())
        safe = fallback;

    return safe;
}

#endif // UTILS_H
