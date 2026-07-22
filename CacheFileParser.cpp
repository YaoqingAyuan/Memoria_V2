#include "CacheFileParser.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>

CacheFileParser::CacheFileParser() {}

//文件夹(Cathe)解析(Cathe_Parse)函数:解析文件夹(文件树结构)
//工作流步骤1:递归找到所有文件路径(entryJsonPath, indexJsonPath, videoFilePath, audioFilePath)
bool CacheFileParser::Cathe_Parse(const QString &folderPath, VideoInfo &outVideoInfo) {
    Logger::instance()->debug("Parser", QString(">>> 开始扫描目录: %1").arg(folderPath));

    QDir dir(folderPath);
    if (!dir.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：目录不存在！请检查路径配置: %1").arg(folderPath));
        return false;
    }

    outVideoInfo = VideoInfo();
    outVideoInfo.cacheRootPath = folderPath;

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subDirs.isEmpty()) {
        Logger::instance()->critical("Parser", "❌ 错误：缓存目录为空，未找到子目录");
        return false;
    }

    QString contentDir = subDirs.first();
    QString contentPathStr = dir.filePath(contentDir);
    Logger::instance()->debug("Parser", QString("✅ 找到内容子目录: %1").arg(contentDir));

    //步骤1.1:查找entry.json路径
    QString entryJsonPath = QDir(contentPathStr).filePath("entry.json");
    QFileInfo entryFileInfo(entryJsonPath);
    if (!entryFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：entry.json 文件不存在: %1").arg(entryJsonPath));
        return false;
    }
    outVideoInfo.entryJsonPath = entryFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到 entry.json: %1").arg(outVideoInfo.entryJsonPath));

    //步骤1.2:查找index.json路径和音视频文件路径(调用findMediaFiles)
    if (!findMediaFiles(contentPathStr, outVideoInfo)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：未找到视频或音频文件");
        return false;
    }

    //步骤2:展平JSON文件(先展平，后解析)
    if (!EntryflattenJson(outVideoInfo.entryJsonPath)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：展平 entry.json 失败");
        return false;
    }

    if (!indexflattenJson(outVideoInfo.indexJsonPath)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：展平 index.json 失败");
        return false;
    }

    //步骤3:解析展平后的数据到结构体
    StreamInfo videoStream, audioStream;
    if (!parseEntryJson(outVideoInfo)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 entry.json 失败");
        return false;
    }

    if (!parseIndexJson(outVideoInfo, videoStream, audioStream)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 index.json 失败");
        return false;
    }

    //步骤4:计算目录大小(嵌入到解析流程中)
    outVideoInfo.totalBytes = getDirectorySize(dir);
    outVideoInfo.downloadedBytes = outVideoInfo.totalBytes;

    Logger::instance()->debug("Parser", QString("✅ 解析完成！视频标题: %1, AVID: %2").arg(outVideoInfo.title).arg(outVideoInfo.avid));
    Logger::instance()->debug("Parser", QString("✅ 视频文件: %1").arg(outVideoInfo.videoFilePath));
    Logger::instance()->debug("Parser", QString("✅ 音频文件: %1").arg(outVideoInfo.audioFilePath));

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
bool CacheFileParser::EntryflattenJson(const QString &filePath) {
    QFile file(filePath);

    if (!file.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：entry.json 不存在: %1").arg(filePath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->critical("Parser", QString("❌ 错误：无法打开 entry.json: %1, 原因: %2").arg(filePath).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：entry.json 格式损坏或为空: %1").arg(filePath));
        return false;
    }

    EntryJsonData.clear();
    flattenJsonRecursive(jsonDoc.object(), "", EntryJsonData);

    Logger::instance()->debug("Parser", QString("✅ entry.json 展平完成，共 %1 个字段").arg(EntryJsonData.size()));
    return true;
}

//EntryJson解析(parse)函数:从容器EntryJsonData读取→填入VideoInfo结构体
bool CacheFileParser::parseEntryJson(VideoInfo &info) {
    info.avid = EntryJsonData["avid"].toLongLong();
    info.bvid = EntryJsonData["bvid"];
    info.title = EntryJsonData["title"];
    info.ownerName = EntryJsonData["owner_name"];
    info.coverUrl = EntryJsonData["cover"];
    info.videoQuality = EntryJsonData["video_quality"].toInt();
    info.qualityDescription = EntryJsonData["quality_pithy_description"];
    info.totalTimeMilli = EntryJsonData["total_time_milli"].toLongLong();
    info.page_ep_Data.cid = EntryJsonData["page_data.cid"].toLongLong();
    info.page_ep_Data.page = EntryJsonData["page_data.page"].toInt();
    info.page_ep_Data.partTitle = EntryJsonData["page_data.part"];
    info.page_ep_Data.link = EntryJsonData["page_data.link"];
    info.page_ep_Data.width = EntryJsonData["page_data.width"].toInt();
    info.page_ep_Data.height = EntryJsonData["page_data.height"].toInt();
    info.page_ep_Data.rotate = EntryJsonData["page_data.rotate"].toInt();

    if (info.title.isEmpty()) {
        Logger::instance()->warning("Parser", "⚠️ 警告：解析到的标题为空");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析视频标题: %1").arg(info.title));
    }

    if (info.avid == 0) {
        Logger::instance()->warning("Parser", "⚠️ 警告：解析到的 AVID 为空");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析 AVID: %1").arg(info.avid));
    }

    if (!EntryJsonData.contains("page_data.cid")) {
        Logger::instance()->warning("Parser", "⚠️ 警告：未找到 page_data 字段");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析页面数据: CID=%1, 宽=%2, 高=%3").arg(info.page_ep_Data.cid).arg(info.page_ep_Data.width).arg(info.page_ep_Data.height));
    }

    return true;
}

//index.json展平(flatten)函数:读取文件+递归展平→送入容器IndexJsonData
bool CacheFileParser::indexflattenJson(const QString &filePath) {
    QFile file(filePath);

    if (!file.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：index.json 不存在: %1").arg(filePath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->critical("Parser", QString("❌ 错误：无法打开 index.json: %1, 原因: %2").arg(filePath).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：index.json 格式损坏或为空: %1").arg(filePath));
        return false;
    }

    IndexJsonData.clear();
    flattenJsonRecursive(jsonDoc.object(), "", IndexJsonData);

    Logger::instance()->debug("Parser", QString("✅ index.json 展平完成，共 %1 个字段").arg(IndexJsonData.size()));
    return true;
}

//IndexJson解析(parse)函数:从容器IndexJsonData读取→填入StreamInfo结构体
bool CacheFileParser::parseIndexJson(VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream) {
    videoStream.id = IndexJsonData["video[0].id"].toInt();
    videoStream.bandwidth = IndexJsonData["video[0].bandwidth"].toInt();
    videoStream.codecid = IndexJsonData["video[0].codecid"].toInt();
    videoStream.md5 = IndexJsonData["video[0].md5"];
    videoStream.size = IndexJsonData["video[0].size"].toLongLong();
    videoStream.FrameRate = IndexJsonData["video[0].frame_rate"];
    videoStream.width = IndexJsonData["video[0].width"].toInt();
    videoStream.height = IndexJsonData["video[0].height"].toInt();

    Logger::instance()->debug("Parser", QString("✅ 解析视频流信息: 宽=%1, 高=%2, 码率=%3").arg(videoStream.width).arg(videoStream.height).arg(videoStream.bandwidth));

    audioStream.id = IndexJsonData["audio[0].id"].toInt();
    audioStream.bandwidth = IndexJsonData["audio[0].bandwidth"].toInt();
    audioStream.codecid = IndexJsonData["audio[0].codecid"].toInt();
    audioStream.md5 = IndexJsonData["audio[0].md5"];
    audioStream.size = IndexJsonData["audio[0].size"].toLongLong();

    Logger::instance()->debug("Parser", QString("✅ 解析音频流信息: 码率=%1, 大小=%2").arg(audioStream.bandwidth).arg(audioStream.size));

    return true;
}

//寻找媒体(Media)文件(Files):找到index.json、video.m4s、audio.m4s文件路径
bool CacheFileParser::findMediaFiles(const QString &dirPath, VideoInfo &info) {
    QDir contentDir(dirPath);
    QStringList subDirs = contentDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (subDirs.isEmpty()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：内容目录 %1 下未找到画质子目录").arg(dirPath));
        return false;
    }

    QString qualityDir = subDirs.first();
    QDir qualityPath = contentDir.filePath(qualityDir);

    //查找index.json
    QFileInfo indexFileInfo(qualityPath.filePath("index.json"));
    if (!indexFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：index.json 文件不存在: %1").arg(indexFileInfo.absoluteFilePath()));
        return false;
    }
    info.indexJsonPath = indexFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到 index.json: %1").arg(info.indexJsonPath));

    //查找video.m4s
    QFileInfo videoFileInfo(qualityPath.filePath("video.m4s"));
    if (!videoFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：video.m4s 文件不存在: %1").arg(videoFileInfo.absoluteFilePath()));
        return false;
    }
    info.videoFilePath = videoFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到视频文件: %1").arg(info.videoFilePath));

    //查找audio.m4s
    QFileInfo audioFileInfo(qualityPath.filePath("audio.m4s"));
    if (!audioFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：audio.m4s 文件不存在: %1").arg(audioFileInfo.absoluteFilePath()));
        return false;
    }
    info.audioFilePath = audioFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到音频文件: %1").arg(info.audioFilePath));

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
