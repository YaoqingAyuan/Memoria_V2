#ifndef CACHEFILEPARSER_H
#define CACHEFILEPARSER_H
//缓存(Cache)文件(File)解析器(Parser)类

#include <QString>
#include <QDir>
#include <QJsonObject>

struct VideoInfo {
    qint64 avid;
    QString bvid;
    QString title;
    QString ownerName;
    QString coverUrl;
    int videoQuality;
    QString qualityDescription;
    qint64 totalTimeMilli;
    qint64 totalBytes;
    qint64 downloadedBytes;

    struct PageData {
        qint64 cid;
        int page;
        QString partTitle;
        QString link;
        int width;
        int height;
        int rotate;
    };
    PageData pageData;

    QString cacheRootPath;
    QString entryJsonPath;
    QString indexJsonPath;
    QString videoFilePath;
    QString audioFilePath;

    bool isValid() const {
        return !videoFilePath.isEmpty() && !audioFilePath.isEmpty();
    }
};

struct StreamInfo {
    int id;
    int bandwidth;
    int codecid;
    QString md5;
    qint64 size;
    QString frameRate;
    int width;
    int height;
};

class CacheFileParser
{
public:
    CacheFileParser();

    CacheFileParser();

    bool parse(const QString &folderPath, VideoInfo &outVideoInfo);
    bool parseEntryJson(const QDir &dir, VideoInfo &info);
    bool parseIndexJson(const QDir &dir, VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream);
    bool findMediaFiles(const QDir &dir, const QString &qualityDir, VideoInfo &info);
    qint64 getDirectorySize(const QDir &dir);

private:
    void logDebug(const QString &msg);
    void logWarning(const QString &msg);
    void logCritical(const QString &msg);
};

#endif // CACHEFILEPARSER_H
