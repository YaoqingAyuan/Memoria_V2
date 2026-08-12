#include "CacheManager.h"
#include "logger.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSettings>
#include <QDirIterator>
#include <QStandardPaths>
#include <QCoreApplication>

//=== 单例 ===

CacheManager& CacheManager::instance()
{
    static CacheManager s_instance;
    return s_instance;
}

//=== 缓存目录 ===

QString CacheManager::cacheDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty()) {
        //回退：若 AppLocalData 不可用，使用应用目录下的 cache 子目录
        dir = QCoreApplication::applicationDirPath() + "/cache";
    }
    dir += "/adb_cache";
    QDir().mkpath(dir);
    return dir;
}

//=== 按项操作 ===

void CacheManager::deleteFolder(const QString &folderName)
{
    if (folderName.isEmpty()) return;

    QString folderPath = cacheDir() + "/" + folderName;
    QDir dir(folderPath);
    if (!dir.exists()) {
        Logger::instance()->debug("CacheManager",
            QString("跳过删除（文件夹不存在）: %1").arg(folderName));
        return;
    }

    if (dir.removeRecursively()) {
        Logger::instance()->debug("CacheManager",
            QString("已删除缓存文件夹: %1").arg(folderName));
    } else {
        Logger::instance()->warning("CacheManager",
            QString("删除缓存文件夹失败: %1").arg(folderName));
    }
}

bool CacheManager::isInCacheDir(const QString &path) const
{
    if (path.isEmpty()) return false;
    QString normalizedPath = QDir::cleanPath(path);
    QString normalizedCache = QDir::cleanPath(cacheDir());
    return normalizedPath.startsWith(normalizedCache, Qt::CaseInsensitive);
}

QString CacheManager::folderNameFromPath(const QString &path) const
{
    if (!isInCacheDir(path)) return QString();

    //缓存结构：.../adb_cache/<folderName>/<subDir>/<files>
    //需要提取 <folderName>，即相对于 cacheDir 的第一级目录
    QString relative = QDir::cleanPath(path);
    QString cacheBase = QDir::cleanPath(cacheDir());

    //去掉 cacheDir 前缀
    if (relative.startsWith(cacheBase, Qt::CaseInsensitive)) {
        relative = relative.mid(cacheBase.length());
        //去掉前导斜杠
        while (relative.startsWith('/'))
            relative = relative.mid(1);
        //取第一级目录名
        int slashPos = relative.indexOf('/');
        if (slashPos > 0)
            return relative.left(slashPos);
        else
            return relative;
    }

    return QString();
}

//=== 全量操作 ===

void CacheManager::cleanAll()
{
    QDir dir(cacheDir());
    if (!dir.exists()) return;

    int count = 0;
    QDirIterator it(dir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString subDirPath = it.next();
        if (QDir(subDirPath).removeRecursively())
            count++;
    }

    Logger::instance()->debug("CacheManager",
        QString("已清理全部缓存：%1 个文件夹").arg(count));
}

qint64 CacheManager::cacheSize() const
{
    QDir dir(cacheDir());
    if (!dir.exists()) return 0;

    qint64 total = 0;
    QDirIterator it(dir.absolutePath(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString CacheManager::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
}

//=== 过期清理 ===

void CacheManager::cleanExpired(int maxDays)
{
    if (maxDays <= 0) return;

    QDir dir(cacheDir());
    if (!dir.exists()) return;

    QDateTime threshold = QDateTime::currentDateTime().addDays(-maxDays);
    int count = 0;

    QDirIterator it(dir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString subDirPath = it.next();
        QFileInfo fi(subDirPath);
        //以文件夹的最后修改时间判断是否过期
        if (fi.lastModified() < threshold) {
            if (QDir(subDirPath).removeRecursively()) {
                count++;
                Logger::instance()->debug("CacheManager",
                    QString("过期清理: %1 (修改于 %2)")
                        .arg(fi.fileName())
                        .arg(fi.lastModified().toString("yyyy-MM-dd HH:mm")));
            }
        }
    }

    if (count > 0) {
        Logger::instance()->debug("CacheManager",
            QString("过期清理完成：删除 %1 个文件夹（超过 %2 天）").arg(count).arg(maxDays));
    }
}

//=== 设置 ===

bool CacheManager::cleanOnClose() const
{
    QSettings s;
    return s.value("cache/cleanOnClose", false).toBool();
}

void CacheManager::setCleanOnClose(bool enabled)
{
    QSettings s;
    s.setValue("cache/cleanOnClose", enabled);
}

int CacheManager::expiryDays() const
{
    QSettings s;
    int days = s.value("cache/expiryDays", 7).toInt();
    return qBound(1, days, 30);
}

void CacheManager::setExpiryDays(int days)
{
    days = qBound(1, days, 30);
    QSettings s;
    s.setValue("cache/expiryDays", days);
}
