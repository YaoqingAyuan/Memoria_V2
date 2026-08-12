#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H
//缓存(Cache)管理器(Manager)：管理ADB拉取缓存的生命周期
//职责：缓存目录管理、按项删除、全量清理、过期清理、大小计算
//存储位置：QStandardPaths::AppLocalDataLocation/adb_cache（不被系统自动清理）

#include <QString>
#include <QSet>

class CacheManager
{
public:
    static CacheManager& instance();

    //=== 缓存目录 ===
    //返回缓存根目录完整路径（确保目录已创建）
    QString cacheDir() const;

    //=== 按项操作 ===
    //删除指定文件夹名的缓存（如 "116451998503195"）
    //若文件夹不存在则静默跳过
    void deleteFolder(const QString &folderName);

    //判断路径是否位于缓存目录内（用于区分ADB缓存导入 vs 本地文件导入）
    bool isInCacheDir(const QString &path) const;

    //从完整路径中提取缓存文件夹名（如 ".../adb_cache/123456/80/video.m4s" → "123456"）
    //若路径不在缓存目录内，返回空字符串
    QString folderNameFromPath(const QString &path) const;

    //=== 全量操作 ===
    //删除缓存目录下的所有文件夹
    void cleanAll();

    //计算缓存目录总大小（字节）
    qint64 cacheSize() const;

    //格式化大小为人类可读字符串（如 "123.4 MB"）
    static QString formatSize(qint64 bytes);

    //=== 过期清理 ===
    //删除修改时间超过 maxDays 天的缓存文件夹
    void cleanExpired(int maxDays);

    //=== 设置（持久化到 QSettings）===
    //关闭应用时清理全部缓存
    bool cleanOnClose() const;
    void setCleanOnClose(bool enabled);

    //缓存过期天数（默认7，范围1-30）
    int expiryDays() const;
    void setExpiryDays(int days);

private:
    CacheManager() = default;
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;
};

#endif // CACHEMANAGER_H
