#include "CacheFileParser.h"
#include "ParsedCacheData.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>

/* ============================================================
 *  CacheFileParser 成员函数工作流（字符画）
 * ============================================================
 *
 *  |  Cathe_Parse()   |  <-- 对外入口：解析一个缓存文件夹
 *           ↓
 *  +---------------------------+
 *  | 1.校验目录存在性           |
 *  | 2.初始化 ParsedCacheData   |
 *  +------------+--------------+
 *               ↓
 *  +---------------------------+
 *  | 定位 entry.json 路径       |
 *  +------------+--------------+
 *               ↓
 *  +---------------------------+
 *  |   findMediaFiles()        |  <-- 寻找 index.json / video.m4s / audio.m4s
 *  +------------+--------------+
 *               |
 *     +---------+---------+
 *     ↓                   ↓
 * +----------------+  +----------------+
 * | EntryflattenJson() |  | indexflattenJson() | <--递归展平两Json文件
 * +--------+-------+  +--------+-------+
 *          ↓                   ↓
 *         +---------------------------+
 *         |  flattenJsonRecursive     |  <-- 两个展平函数共同调用flattenJsonRecursive
 *         |  (entry.json&index.json)  |
 *         +---------------------------+
 *          ↓                   ↓
 * +----------------+  +----------------+
 * |outData.entryJsonData|  |outData.indexJsonData |
 * +--------+-------+  +--------+-------+
 *          ↓                   ↓
 * +----------------+  +----------------+
 * | parseEntryJson() |  | parseIndexJson()   | <-- 解析两Json文件数据，填入videoInfo等结构体
 * +--------+-------+  +--------+-------+
 *          ↓                   ↓
 * +----------------+  +------------------+
 * | outData.videoInfo |  | outData.videoStream |
 * +----------------+  | outData.audioStream |
 *                     +------------------+
 *                               ↓
 *                     +------------------+
 *                     | getDirectorySize() |  <-- 递归计算目录总大小
 *                     +--------+---------+
 *                              ↓
 *                     +------------------+
 *                     | 写入 totalBytes   |
 *                     | 写入 downloadedBytes|
 *                     +--------+---------+
 *                              ↓
 *                     +------------------+
 *                     |   返回 true/false  |
 *                     +------------------+
 * ============================================================
 */

CacheFileParser::CacheFileParser() {}

//文件夹(Cathe)解析(Cathe_Parse)函数:解析文件夹(文件树结构)
//工作流步骤1:递归找到所有文件路径(entryJsonPath, indexJsonPath, videoFilePath, audioFilePath)
bool CacheFileParser::Cathe_Parse(const QString &folderPath, ParsedCacheData &outData) {
    Logger::instance()->debug("Parser", QString(">>> 开始扫描目录: %1").arg(folderPath));

    QDir dir(folderPath);
    if (!dir.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：目录不存在！请检查路径配置: %1").arg(folderPath));
        return false;
    }

    outData.videoInfo = VideoInfo();
    outData.videoInfo.cacheRootPath = folderPath;

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
    outData.videoInfo.entryJsonPath = entryFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到 entry.json: %1").arg(outData.videoInfo.entryJsonPath));

    //步骤1.2:查找index.json路径和音视频文件路径(调用findMediaFiles)
    if (!findMediaFiles(contentPathStr, outData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：未找到视频或音频文件");
        return false;
    }

    //步骤2:展平JSON文件(先展平，后解析)
    if (!EntryflattenJson(outData.videoInfo.entryJsonPath, outData.entryJsonData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：展平 entry.json 失败");
        return false;
    }

    if (!indexflattenJson(outData.videoInfo.indexJsonPath, outData.indexJsonData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：展平 index.json 失败");
        return false;
    }

    //步骤3:解析展平后的数据到结构体
    if (!parseEntryJson(outData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 entry.json 失败");
        return false;
    }

    if (!parseIndexJson(outData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 index.json 失败");
        return false;
    }

    //步骤4:计算目录大小(嵌入到解析流程中)
    outData.videoInfo.totalBytes = getDirectorySize(dir);
    outData.videoInfo.downloadedBytes = outData.videoInfo.totalBytes;

    Logger::instance()->debug("Parser", QString("✅ 解析完成！视频标题: %1, AVID: %2").arg(outData.videoInfo.title).arg(outData.videoInfo.avid));
    Logger::instance()->debug("Parser", QString("✅ 视频文件: %1").arg(outData.videoInfo.videoFilePath));
    Logger::instance()->debug("Parser", QString("✅ 音频文件: %1").arg(outData.videoInfo.audioFilePath));

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
bool CacheFileParser::EntryflattenJson(const QString &filePath, MetadataContainer &container) {
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

    container.clear();
    flattenJsonRecursive(jsonDoc.object(), "", container);

    Logger::instance()->debug("Parser", QString("✅ entry.json 展平完成，共 %1 个字段").arg(container.size()));
    return true;
}

//EntryJson解析(parse)函数:从容器EntryJsonData读取→填入VideoInfo结构体
bool CacheFileParser::parseEntryJson(ParsedCacheData &data) {
    data.videoInfo.avid = data.entryJsonData["avid"].toLongLong();
    data.videoInfo.bvid = data.entryJsonData["bvid"];
    data.videoInfo.title = data.entryJsonData["title"];
    data.videoInfo.ownerName = data.entryJsonData["owner_name"];
    data.videoInfo.coverUrl = data.entryJsonData["cover"];
    data.videoInfo.videoQuality = data.entryJsonData["video_quality"].toInt();
    data.videoInfo.qualityDescription = data.entryJsonData["quality_pithy_description"];
    data.videoInfo.totalTimeMilli = data.entryJsonData["total_time_milli"].toLongLong();
    data.videoInfo.page_ep_Data.cid = data.entryJsonData["page_data.cid"].toLongLong();
    data.videoInfo.page_ep_Data.page = data.entryJsonData["page_data.page"].toInt();
    data.videoInfo.page_ep_Data.partTitle = data.entryJsonData["page_data.part"];
    data.videoInfo.page_ep_Data.link = data.entryJsonData["page_data.link"];
    data.videoInfo.page_ep_Data.width = data.entryJsonData["page_data.width"].toInt();
    data.videoInfo.page_ep_Data.height = data.entryJsonData["page_data.height"].toInt();
    data.videoInfo.page_ep_Data.rotate = data.entryJsonData["page_data.rotate"].toInt();

    if (data.videoInfo.title.isEmpty()) {
        Logger::instance()->warning("Parser", "⚠️ 警告：解析到的标题为空");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析视频标题: %1").arg(data.videoInfo.title));
    }

    if (data.videoInfo.avid == 0) {
        Logger::instance()->warning("Parser", "⚠️ 警告：解析到的 AVID 为空");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析 AVID: %1").arg(data.videoInfo.avid));
    }

    if (!data.entryJsonData.contains("page_data.cid")) {
        Logger::instance()->warning("Parser", "⚠️ 警告：未找到 page_data 字段");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析页面数据: CID=%1, 宽=%2, 高=%3").arg(data.videoInfo.page_ep_Data.cid).arg(data.videoInfo.page_ep_Data.width).arg(data.videoInfo.page_ep_Data.height));
    }

    return true;
}

//index.json展平(flatten)函数:读取文件+递归展平→送入容器IndexJsonData
bool CacheFileParser::indexflattenJson(const QString &filePath, MetadataContainer &container) {
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

    container.clear();
    flattenJsonRecursive(jsonDoc.object(), "", container);

    Logger::instance()->debug("Parser", QString("✅ index.json 展平完成，共 %1 个字段").arg(container.size()));
    return true;
}

//IndexJson解析(parse)函数:从容器IndexJsonData读取→填入StreamInfo结构体
bool CacheFileParser::parseIndexJson(ParsedCacheData &data) {
    data.videoStream.id = data.indexJsonData["video[0].id"].toInt();
    data.videoStream.bandwidth = data.indexJsonData["video[0].bandwidth"].toInt();
    data.videoStream.codecid = data.indexJsonData["video[0].codecid"].toInt();
    data.videoStream.md5 = data.indexJsonData["video[0].md5"];
    data.videoStream.size = data.indexJsonData["video[0].size"].toLongLong();
    data.videoStream.FrameRate = data.indexJsonData["video[0].frame_rate"];
    data.videoStream.width = data.indexJsonData["video[0].width"].toInt();
    data.videoStream.height = data.indexJsonData["video[0].height"].toInt();

    Logger::instance()->debug("Parser", QString("✅ 解析视频流信息: 宽=%1, 高=%2, 码率=%3").arg(data.videoStream.width).arg(data.videoStream.height).arg(data.videoStream.bandwidth));

    data.audioStream.id = data.indexJsonData["audio[0].id"].toInt();
    data.audioStream.bandwidth = data.indexJsonData["audio[0].bandwidth"].toInt();
    data.audioStream.codecid = data.indexJsonData["audio[0].codecid"].toInt();
    data.audioStream.md5 = data.indexJsonData["audio[0].md5"];
    data.audioStream.size = data.indexJsonData["audio[0].size"].toLongLong();

    Logger::instance()->debug("Parser", QString("✅ 解析音频流信息: 码率=%1, 大小=%2").arg(data.audioStream.bandwidth).arg(data.audioStream.size));

    return true;
}

//寻找媒体(Media)文件(Files):找到index.json、video.m4s、audio.m4s文件路径
bool CacheFileParser::findMediaFiles(const QString &dirPath, ParsedCacheData &data) {
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
    data.videoInfo.indexJsonPath = indexFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到 index.json: %1").arg(data.videoInfo.indexJsonPath));

    //查找video.m4s
    QFileInfo videoFileInfo(qualityPath.filePath("video.m4s"));
    if (!videoFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：video.m4s 文件不存在: %1").arg(videoFileInfo.absoluteFilePath()));
        return false;
    }
    data.videoInfo.videoFilePath = videoFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到视频文件: %1").arg(data.videoInfo.videoFilePath));

    //查找audio.m4s
    QFileInfo audioFileInfo(qualityPath.filePath("audio.m4s"));
    if (!audioFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：audio.m4s 文件不存在: %1").arg(audioFileInfo.absoluteFilePath()));
        return false;
    }
    data.videoInfo.audioFilePath = audioFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到音频文件: %1").arg(data.videoInfo.audioFilePath));

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
