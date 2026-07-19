#include "CacheFileParser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>

CacheFileParser::CacheFileParser() {}

//文件夹(Cathe)解析(Cathe_Parse)函数:解析文件夹(文件树结构)
bool CacheFileParser::Cathe_Parse(const QString &folderPath, VideoInfo &outVideoInfo) {
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

//递归展平JSON函数(通用辅助函数)
void CacheFileParser::flattenJsonRecursive(const QJsonObject &obj, const QString &prefix, MetadataContainer &container) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        const QJsonValue &value = it.value();

        if (value.isObject()) {
            flattenJsonRecursive(value.toObject(), key, container);
        } else if (value.isArray()) {
            QJsonArray arr = value.toArray();
            QStringList arrValues;
            for (int i = 0; i < arr.size(); ++i) {
                if (arr[i].isObject()) {
                    flattenJsonRecursive(arr[i].toObject(), key + "[" + QString::number(i) + "]", container);
                } else {
                    arrValues.append(arr[i].toString());
                }
            }
            if (!arrValues.isEmpty()) {
                container[key] = arrValues.join(", ");
            }
        } else {
            QString strValue = value.toString();
            if (value.isBool()) {
                strValue = value.toBool() ? "true" : "false";
            } else if (value.isDouble()) {
                strValue = QString::number(value.toDouble());
            }
            container[key] = strValue;
        }
    }
}

//Entry.json展平(flatten)函数:读取文件+递归展平→送入容器EntryJsonData
bool CacheFileParser::EntryflattenJson(const QDir &dir) {
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

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        logCritical(QString("[Parser] ❌ 错误：entry.json 格式损坏或为空: %1").arg(entryPath));
        return false;
    }

    EntryJsonData.clear();
    flattenJsonRecursive(jsonDoc.object(), "", EntryJsonData);

    logDebug(QString("[Parser] ✅ entry.json 展平完成，共 %1 个字段").arg(EntryJsonData.size()));
    return true;
}

//EntryJson解析(parse)函数:从容器EntryJsonData读取→填入VideoInfo结构体
bool CacheFileParser::parseEntryJson(const QDir &dir, VideoInfo &info) {
    if (!EntryflattenJson(dir)) {
        return false;
    }

    info.entryJsonPath = dir.filePath("entry.json");

    info.avid = EntryJsonData["avid"].toLongLong();
    info.bvid = EntryJsonData["bvid"];
    info.title = EntryJsonData["title"];
    info.ownerName = EntryJsonData["owner_name"];
    info.coverUrl = EntryJsonData["cover"];
    info.videoQuality = EntryJsonData["video_quality"].toInt();
    info.qualityDescription = EntryJsonData["quality_pithy_description"];
    info.totalTimeMilli = EntryJsonData["total_time_milli"].toLongLong();
    info.totalBytes = EntryJsonData["total_bytes"].toLongLong();
    info.downloadedBytes = EntryJsonData["downloaded_bytes"].toLongLong();

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

    info.pageData.cid = EntryJsonData["page_data.cid"].toLongLong();
    info.pageData.page = EntryJsonData["page_data.page"].toInt();
    info.pageData.partTitle = EntryJsonData["page_data.part"];
    info.pageData.link = EntryJsonData["page_data.link"];
    info.pageData.width = EntryJsonData["page_data.width"].toInt();
    info.pageData.height = EntryJsonData["page_data.height"].toInt();
    info.pageData.rotate = EntryJsonData["page_data.rotate"].toInt();

    if (!EntryJsonData.contains("page_data.cid")) {
        logWarning("[Parser] ⚠️ 警告：未找到 page_data 字段");
    } else {
        logDebug(QString("[Parser] ✅ 成功解析页面数据: CID=%1, 宽=%2, 高=%3").arg(info.pageData.cid).arg(info.pageData.width).arg(info.pageData.height));
    }

    return true;
}

//index.json展平(flatten)函数:读取文件+递归展平→送入容器IndexJsonData
bool CacheFileParser::indexflattenJson(const QDir &dir) {
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

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        logCritical(QString("[Parser] ❌ 错误：index.json 格式损坏或为空: %1").arg(indexPath));
        return false;
    }

    IndexJsonData.clear();
    flattenJsonRecursive(jsonDoc.object(), "", IndexJsonData);

    logDebug(QString("[Parser] ✅ index.json 展平完成，共 %1 个字段").arg(IndexJsonData.size()));
    return true;
}

//IndexJson解析(parse)函数:从容器IndexJsonData读取→填入StreamInfo结构体
bool CacheFileParser::parseIndexJson(const QDir &dir, VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream) {
    if (!indexflattenJson(dir)) {
        return false;
    }

    info.indexJsonPath = dir.filePath("index.json");

    videoStream.id = IndexJsonData["video[0].id"].toInt();
    videoStream.bandwidth = IndexJsonData["video[0].bandwidth"].toInt();
    videoStream.codecid = IndexJsonData["video[0].codecid"].toInt();
    videoStream.md5 = IndexJsonData["video[0].md5"];
    videoStream.size = IndexJsonData["video[0].size"].toLongLong();
    videoStream.FrameRate = IndexJsonData["video[0].frame_rate"];
    videoStream.width = IndexJsonData["video[0].width"].toInt();
    videoStream.height = IndexJsonData["video[0].height"].toInt();

    logDebug(QString("[Parser] ✅ 解析视频流信息: 宽=%1, 高=%2, 码率=%3").arg(videoStream.width).arg(videoStream.height).arg(videoStream.bandwidth));

    audioStream.id = IndexJsonData["audio[0].id"].toInt();
    audioStream.bandwidth = IndexJsonData["audio[0].bandwidth"].toInt();
    audioStream.codecid = IndexJsonData["audio[0].codecid"].toInt();
    audioStream.md5 = IndexJsonData["audio[0].md5"];
    audioStream.size = IndexJsonData["audio[0].size"].toLongLong();

    logDebug(QString("[Parser] ✅ 解析音频流信息: 码率=%1, 大小=%2").arg(audioStream.bandwidth).arg(audioStream.size));

    return true;
}

//寻找媒体(Media)文件(Files):找到音视频.m4s文件
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

//获取(get)字段(Directory)大小(Size)
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

//三个控制台提示信息函数
void CacheFileParser::logDebug(const QString &msg) {
    qDebug() << msg;
}

void CacheFileParser::logWarning(const QString &msg) {
    qWarning() << msg;
}

void CacheFileParser::logCritical(const QString &msg) {
    qCritical() << msg;
}