#ifndef UTILS_H
#define UTILS_H
//工具类（简单工具，无需封装）

#include <QString>
#include <QFile>
#include <QCryptographicHash>
#include <QByteArrayView>

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

#endif // UTILS_H
