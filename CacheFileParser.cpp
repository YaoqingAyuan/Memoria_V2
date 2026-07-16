#include "CacheFileParser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
//缓存(Cache)文件(File)解析器(Parser)类

CacheFileParser::CacheFileParser() {}

//解析(parse)函数
bool CacheFileParser::parse(const QString &folderPath, VideoInfo &outVideoInfo) {
    logDebug(QString("[Parser] >>> 开始扫描目录: %1").arg(folderPath));

    QDir dir(folderPath);
    if (!dir.exists()) {
        logCritical(QString("[Parser] ❌ 错误：目录不存在！请检查路径配置: %1").arg(folderPath));
        return false;
    }

    outVideoInfo = VideoInfo();
    outVideoInfo.cacheRootPath = folderPath;

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subDirs.isEmpty()) {
        logCritical("[Parser] ❌ 错误：缓存目录为空，未找到子目录");
        return false;
    }

    QString contentDir = subDirs.first();
    QDir contentPath = dir.filePath(contentDir);
    logDebug(QString("[Parser] ✅ 找到内容子目录: %1").arg(contentDir));

    if (!parseEntryJson(contentPath, outVideoInfo)) {
        logCritical("[Parser] ❌ 致命错误：解析 entry.json 失败");
        return false;
    }

    QString qualityDir = QString::number(outVideoInfo.videoQuality);
    QDir qualityPath = contentPath.filePath(qualityDir);
    if (!qualityPath.exists()) {
        logCritical(QString("[Parser] ❌ 错误：未找到画质目录 %1").arg(qualityDir));
        return false;
    }

    StreamInfo videoStream, audioStream;
    if (!parseIndexJson(qualityPath, outVideoInfo, videoStream, audioStream)) {
        logCritical("[Parser] ❌ 致命错误：解析 index.json 失败");
        return false;
    }

    if (!findMediaFiles(qualityPath, qualityDir, outVideoInfo)) {
        logCritical("[Parser] ❌ 致命错误：未找到视频或音频文件");
        return false;
    }

    outVideoInfo.totalBytes = getDirectorySize(dir);
    outVideoInfo.downloadedBytes = outVideoInfo.totalBytes;

    logDebug(QString("[Parser] ✅ 解析完成！视频标题: %1, AVID: %2").arg(outVideoInfo.title).arg(outVideoInfo.avid));
    logDebug(QString("[Parser] ✅ 视频文件: %1").arg(outVideoInfo.videoFilePath));
    logDebug(QString("[Parser] ✅ 音频文件: %1").arg(outVideoInfo.audioFilePath));

    return true;
}

//解析(parse)Entry.Json文件
bool CacheFileParser::parseEntryJson(const QDir &dir, VideoInfo &info) {
    QString entryPath = dir.filePath("entry.json");
    QFile file(entryPath);

    if (!file.exists()) {
        logCritical(QString("[Parser] ❌ 错误：entry.json 不存在: %1").arg(entryPath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        logCritical(QString("[Parser] ❌ 错误：无法打开 entry.json: %1, 原因: %2").arg(entryPath).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();
    info.entryJsonPath = entryPath;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        logCritical(QString("[Parser] ❌ 错误：entry.json 格式损坏或为空: %1").arg(entryPath));
        return false;
    }

    QJsonObject jsonObj = jsonDoc.object();

    info.avid = jsonObj["avid"].toVariant().toLongLong();
    info.bvid = jsonObj["bvid"].toString();
    info.title = jsonObj["title"].toString();
    info.ownerName = jsonObj["owner_name"].toString();
    info.coverUrl = jsonObj["cover"].toString();
    info.videoQuality = jsonObj["video_quality"].toInt();
    info.qualityDescription = jsonObj["quality_pithy_description"].toString();
    info.totalTimeMilli = jsonObj["total_time_milli"].toVariant().toLongLong();
    info.totalBytes = jsonObj["total_bytes"].toVariant().toLongLong();
    info.downloadedBytes = jsonObj["downloaded_bytes"].toVariant().toLongLong();

    if (info.title.isEmpty()) {
        logWarning("[Parser] ⚠️ 警告：解析到的标题为空");
    } else {
        logDebug(QString("[Parser] ✅ 成功解析视频标题: %1").arg(info.title));
    }

    if (info.avid == 0) {
        logWarning("[Parser] ⚠️ 警告：解析到的 AVID 为空");
    } else {
        logDebug(QString("[Parser] ✅ 成功解析 AVID: %1").arg(info.avid));
    }

    if (jsonObj.contains("page_data") && jsonObj["page_data"].isObject()) {
        QJsonObject pageObj = jsonObj["page_data"].toObject();
        info.pageData.cid = pageObj["cid"].toVariant().toLongLong();
        info.pageData.page = pageObj["page"].toInt();
        info.pageData.partTitle = pageObj["part"].toString();
        info.pageData.link = pageObj["link"].toString();
        info.pageData.width = pageObj["width"].toInt();
        info.pageData.height = pageObj["height"].toInt();
        info.pageData.rotate = pageObj["rotate"].toInt();

        logDebug(QString("[Parser] ✅ 成功解析页面数据: CID=%1, 宽=%2, 高=%3").arg(info.pageData.cid).arg(info.pageData.width).arg(info.pageData.height));
    } else {
        logWarning("[Parser] ⚠️ 警告：未找到 page_data 字段");
    }

    return true;
}

//解析(parse)Index.Json函数【感觉没必要，毕竟最核心标题等重要信息在Entry.json里面】
bool CacheFileParser::parseIndexJson(const QDir &dir, VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream) {
    QString indexPath = dir.filePath("index.json");
    QFile file(indexPath);

    if (!file.exists()) {
        logCritical(QString("[Parser] ❌ 错误：index.json 不存在: %1").arg(indexPath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        logCritical(QString("[Parser] ❌ 错误：无法打开 index.json: %1, 原因: %2").arg(indexPath).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();
    info.indexJsonPath = indexPath;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        logCritical(QString("[Parser] ❌ 错误：index.json 格式损坏或为空: %1").arg(indexPath));
        return false;
    }

    QJsonObject jsonObj = jsonDoc.object();

    if (jsonObj.contains("video") && jsonObj["video"].isArray()) {
        QJsonArray videoArray = jsonObj["video"].toArray();
        if (!videoArray.isEmpty()) {
            QJsonObject videoObj = videoArray.first().toObject();
            videoStream.id = videoObj["id"].toInt();
            videoStream.bandwidth = videoObj["bandwidth"].toInt();
            videoStream.codecid = videoObj["codecid"].toInt();
            videoStream.md5 = videoObj["md5"].toString();
            videoStream.size = videoObj["size"].toVariant().toLongLong();
            videoStream.FrameRate = videoObj["frame_rate"].toString();
            videoStream.width = videoObj["width"].toInt();
            videoStream.height = videoObj["height"].toInt();

            logDebug(QString("[Parser] ✅ 解析视频流信息: 宽=%1, 高=%2, 码率=%3").arg(videoStream.width).arg(videoStream.height).arg(videoStream.bandwidth));
        }
    }

    if (jsonObj.contains("audio") && jsonObj["audio"].isArray()) {
        QJsonArray audioArray = jsonObj["audio"].toArray();
        if (!audioArray.isEmpty()) {
            QJsonObject audioObj = audioArray.first().toObject();
            audioStream.id = audioObj["id"].toInt();
            audioStream.bandwidth = audioObj["bandwidth"].toInt();
            audioStream.codecid = audioObj["codecid"].toInt();
            audioStream.md5 = audioObj["md5"].toString();
            audioStream.size = audioObj["size"].toVariant().toLongLong();

            logDebug(QString("[Parser] ✅ 解析音频流信息: 码率=%1, 大小=%2").arg(audioStream.bandwidth).arg(audioStream.size));
        }
    }

    return true;
}

//寻找(find)媒体(Media)文件(Files),即音视频.m4s
bool CacheFileParser::findMediaFiles(const QDir &dir, const QString &qualityDir, VideoInfo &info) {
    QFileInfo videoFile(dir.filePath("video.m4s"));
    QFileInfo audioFile(dir.filePath("audio.m4s"));

    if (!videoFile.exists()) {
        logCritical(QString("[Parser] ❌ 错误：video.m4s 文件不存在: %1").arg(videoFile.absoluteFilePath()));
        return false;
    }

    if (!audioFile.exists()) {
        logCritical(QString("[Parser] ❌ 错误：audio.m4s 文件不存在: %1").arg(audioFile.absoluteFilePath()));
        return false;
    }

    info.videoFilePath = videoFile.absoluteFilePath();
    info.audioFilePath = audioFile.absoluteFilePath();

    logDebug(QString("[Parser] ✅ 找到视频文件: %1").arg(info.videoFilePath));
    logDebug(QString("[Parser] ✅ 找到音频文件: %1").arg(info.audioFilePath));

    return true;
}

//获取(get)目录?(Directory)大小(Size)
qint64 CacheFileParser::getDirectorySize(const QDir &dir) {
    qint64 totalSize = 0;
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &file : files) {
        if (file.isDir()) {
            totalSize += getDirectorySize(QDir(file.absoluteFilePath()));
        } else {
            totalSize += file.size();
        }
    }

    return totalSize;
}

//不是哥们？这仨语句理论上应该单独归类(或命名空间)的，毕竟好多模块都得用！
//控制台Debug语句
void CacheFileParser::logDebug(const QString &msg) {
    qDebug() << msg;
}

//控制台Warning(警告)语句
void CacheFileParser::logWarning(const QString &msg) {
    qWarning() << msg;
}

//控制台Critical(错误)语句
void CacheFileParser::logCritical(const QString &msg) {
    qCritical() << msg;
}
