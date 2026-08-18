#include "CacheFileParser.h"
#include "Core/ParsedCacheData.h"
#include "Core/logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>

/* ============================================================
 *  CacheFileParser 成员函数工作流（字符画）
 * ============================================================
 *
 *  |  Cathe_Parse()           |  <-- 对外入口：解析一个离线诊断ID文件夹
 *           ↓
 *  | 1.校验目录存在性           |
 *  | 2.遍历所有子目录(多P)      |
 *           ↓
 *  | 对每个子目录调用:          |
 *  | parseSingleSubDir()      |  <-- 解析单个子目录(c_xxx/番剧集号)
 *           ↓
 *  | 定位 entry.json 路径       |
 *               ↓
 *  |   findMediaFiles()        |  <-- 寻找 index.json / video.m4s / audio.m4s
 *               |
 *               ↓
 *  | loadJsonObject()   |  <-- 读取JSON文件→QJsonObject(替代原展平步骤)
 *               ↓
 *  | parseEntryJson()   |  | parseIndexJson()  | <-- 直接从QJsonObject解析→填入结构体
 *          ↓                   ↓
 *  | outData.videoInfo |  | outData.videoStream |
 *  +----------------+  | outData.audioStream |
 *                               ↓
 *                     | getDirectorySize() |  <-- 递归计算子目录总大小
 *                              ↓
 *                     | 写入 totalBytes   |
 *                     | 写入 downloadedBytes|
 *                              ↓
 *                     | 追加到 outDataList |
 * ============================================================
 */

CacheFileParser::CacheFileParser() {}

//文件夹(Cathe)解析(Cathe_Parse)函数:解析离线诊断ID文件夹(文件树结构)
//遍历所有子目录(多P场景下每个子目录是一个独立的P/集)，每个子目录生成一个ParsedCacheData
bool CacheFileParser::Cathe_Parse(const QString &folderPath, QList<ParsedCacheData> &outDataList) {
    Logger::instance()->debug("Parser", QString(">>> 开始扫描目录: %1").arg(folderPath));

    QDir dir(folderPath);
    if (!dir.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：目录不存在！请检查路径配置: %1").arg(folderPath));
        return false;
    }

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subDirs.isEmpty()) {
        Logger::instance()->critical("Parser", "❌ 错误：缓存目录为空，未找到子目录");
        return false;
    }

    Logger::instance()->debug("Parser", QString("发现 %1 个子目录").arg(subDirs.size()));

    //遍历所有子目录，每个子目录解析为一个ParsedCacheData
    int successCount = 0;
    for (const QString &subDir : subDirs) {
        QString subDirPath = dir.filePath(subDir);
        ParsedCacheData data;

        if (parseSingleSubDir(subDirPath, folderPath, data)) {
            outDataList.append(data);
            successCount++;
        } else {
            Logger::instance()->warning("Parser",
                QString("⚠️ 子目录解析失败，已跳过: %1").arg(subDirPath));
        }
    }

    Logger::instance()->debug("Parser",
        QString("✅ 文件夹解析完成: %1 个子目录, 成功 %2 个").arg(subDirs.size()).arg(successCount));

    return successCount > 0;
}

//解析单个子目录(离线诊断ID下的一个c_xxx或番剧集号目录)
bool CacheFileParser::parseSingleSubDir(const QString &subDirPath, const QString &cacheRootPath, ParsedCacheData &outData) {
    outData.videoInfo = VideoInfo();
    outData.videoInfo.cacheRootPath = cacheRootPath;

    QDir subDir(subDirPath);

    //步骤1:查找entry.json路径
    QString entryJsonPath = subDir.filePath("entry.json");
    QFileInfo entryFileInfo(entryJsonPath);
    if (!entryFileInfo.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：entry.json 文件不存在: %1").arg(entryJsonPath));
        return false;
    }
    outData.videoInfo.entryJsonPath = entryFileInfo.absoluteFilePath();
    Logger::instance()->debug("Parser", QString("✅ 找到 entry.json: %1").arg(outData.videoInfo.entryJsonPath));

    //步骤2:查找index.json路径和音视频文件路径(调用findMediaFiles)
    if (!findMediaFiles(subDirPath, outData)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：未找到视频或音频文件");
        return false;
    }

    //步骤3:读取JSON文件为QJsonObject(直接解析，无需展平中间容器)
    QJsonObject entryObj;
    if (!loadJsonObject(outData.videoInfo.entryJsonPath, entryObj)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：读取 entry.json 失败");
        return false;
    }

    QJsonObject indexObj;
    if (!loadJsonObject(outData.videoInfo.indexJsonPath, indexObj)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：读取 index.json 失败");
        return false;
    }

    //步骤4:直接从QJsonObject解析数据到结构体
    if (!parseEntryJson(entryObj, outData.videoInfo)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 entry.json 失败");
        return false;
    }

    if (!parseIndexJson(indexObj, outData.videoStream, outData.audioStream)) {
        Logger::instance()->critical("Parser", "❌ 致命错误：解析 index.json 失败");
        return false;
    }

    //步骤5:计算该子目录大小(每个P独立计算)
    outData.videoInfo.totalBytes = getDirectorySize(subDir);
    outData.videoInfo.downloadedBytes = outData.videoInfo.totalBytes;

    Logger::instance()->debug("Parser", QString("✅ 解析完成！视频标题: %1, AVID: %2").arg(outData.videoInfo.title).arg(outData.videoInfo.avid));
    Logger::instance()->debug("Parser", QString("✅ 视频文件: %1").arg(outData.videoInfo.videoFilePath));
    Logger::instance()->debug("Parser", QString("✅ 音频文件: %1").arg(outData.videoInfo.audioFilePath));

    return true;
}

//读取JSON文件并解析为QJsonObject(替代原展平中间步骤，直接解析)
bool CacheFileParser::loadJsonObject(const QString &filePath, QJsonObject &outObj) {
    QFile file(filePath);

    if (!file.exists()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：文件不存在: %1").arg(filePath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->critical("Parser", QString("❌ 错误：无法打开文件: %1, 原因: %2").arg(filePath).arg(file.errorString()));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        Logger::instance()->critical("Parser", QString("❌ 错误：JSON格式损坏或为空: %1").arg(filePath));
        return false;
    }

    outObj = jsonDoc.object();
    return true;
}

//EntryJson解析(parse)函数：从QJsonObject直接读取→填入VideoInfo结构体
bool CacheFileParser::parseEntryJson(const QJsonObject &obj, VideoInfo &info) {
    info.avid = obj.value("avid").toVariant().toLongLong();
    info.bvid = obj.value("bvid").toString();
    info.title = obj.value("title").toString();
    info.ownerName = obj.value("owner_name").toString();
    info.ownerId = obj.value("owner_id").toString();
    info.coverUrl = obj.value("cover").toString();
    info.videoQuality = obj.value("video_quality").toInt();
    info.qualityDescription = obj.value("quality_pithy_description").toString();
    info.totalTimeMilli = obj.value("total_time_milli").toVariant().toLongLong();
    info.create_timestamp = obj.value("time_create_stamp").toVariant().toLongLong();
    info.recent_danmaku_count = obj.value("danmaku_count").toInt();

    //解析页面/分集数据(兼容普通视频page_data与番剧ep两种结构)
    QJsonObject pageData = obj.value("page_data").toObject();
    if (!pageData.isEmpty()) {
        //普通视频：从page_data读取
        info.videoType = Normal;
        info.page_ep_Data.cid = pageData.value("cid").toVariant().toLongLong();
        info.page_ep_Data.page = pageData.value("page").toInt();
        info.page_ep_Data.partTitle = pageData.value("part").toString();
        info.page_ep_Data.link = pageData.value("link").toString();
        info.page_ep_Data.width = pageData.value("width").toInt();
        info.page_ep_Data.height = pageData.value("height").toInt();
        info.page_ep_Data.rotate = pageData.value("rotate").toInt();
    } else {
        QJsonObject ep = obj.value("ep").toObject();
        if (!ep.isEmpty()) {
            //番剧：从ep读取(cid对应ep.danmaku，page对应ep.index，partTitle对应ep.index_title)
            info.videoType = Bangumi;
            info.page_ep_Data.cid = ep.value("danmaku").toVariant().toLongLong();
            info.page_ep_Data.page = ep.value("index").toInt();
            info.page_ep_Data.partTitle = ep.value("index_title").toString();
            info.page_ep_Data.link = ep.value("link").toString();
            info.page_ep_Data.width = ep.value("width").toInt();
            info.page_ep_Data.height = ep.value("height").toInt();
            info.page_ep_Data.rotate = ep.value("rotate").toInt();
        }
    }

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

    if (info.videoType == Unknown) {
        Logger::instance()->warning("Parser", "⚠️ 警告：未找到 page_data 或 ep 字段");
    } else {
        Logger::instance()->debug("Parser", QString("✅ 成功解析页面数据: CID=%1, 宽=%2, 高=%3").arg(info.page_ep_Data.cid).arg(info.page_ep_Data.width).arg(info.page_ep_Data.height));
    }

    return true;
}

//IndexJson解析(parse)函数：从QJsonObject直接读取→填入StreamInfo结构体
bool CacheFileParser::parseIndexJson(const QJsonObject &obj, StreamInfo &videoStream, StreamInfo &audioStream) {
    //视频流信息(index.json中video是数组，取第一个元素)
    QJsonArray videoArray = obj.value("video").toArray();
    if (!videoArray.isEmpty()) {
        QJsonObject v = videoArray.at(0).toObject();
        videoStream.id = v.value("id").toInt();
        videoStream.bandwidth = v.value("bandwidth").toInt();
        videoStream.codecid = v.value("codecid").toInt();
        videoStream.md5 = v.value("md5").toString();
        videoStream.size = v.value("size").toVariant().toLongLong();
        videoStream.FrameRate = v.value("frame_rate").toString();
        videoStream.width = v.value("width").toInt();
        videoStream.height = v.value("height").toInt();
    }

    Logger::instance()->debug("Parser", QString("✅ 解析视频流信息: 宽=%1, 高=%2, 码率=%3").arg(videoStream.width).arg(videoStream.height).arg(videoStream.bandwidth));

    //音频流信息
    QJsonArray audioArray = obj.value("audio").toArray();
    if (!audioArray.isEmpty()) {
        QJsonObject a = audioArray.at(0).toObject();
        audioStream.id = a.value("id").toInt();
        audioStream.bandwidth = a.value("bandwidth").toInt();
        audioStream.codecid = a.value("codecid").toInt();
        audioStream.md5 = a.value("md5").toString();
        audioStream.size = a.value("size").toVariant().toLongLong();
    }

    Logger::instance()->debug("Parser", QString("✅ 解析音频流信息: 码率=%1, 大小=%2").arg(audioStream.bandwidth).arg(audioStream.size));

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
