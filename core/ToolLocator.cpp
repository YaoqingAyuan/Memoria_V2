#include "ToolLocator.h"
#include "logger.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

QString ToolLocator::locate(const QString &toolName, const QString &bundledSubpath, const QString &logTag)
{
    Logger::instance()->debug(logTag, QString(">>> 开始自检验证：检测%1环境").arg(toolName));

    //策略1：检测系统PATH环境变量中是否有工具(Windows用where命令)
    QProcess probe;
    probe.start("where", QStringList() << toolName);
    bool found = probe.waitForFinished(3000);
    if (found && probe.exitCode() == 0) {
        QString result = QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();
        if (!result.isEmpty() && !result.contains("INFO: Could not find files")) {
            QString path = result.split('\n').first().trimmed();
            Logger::instance()->debug(logTag, QString("✅ 检测到用户环境%1: %2").arg(toolName).arg(path));
            return path;
        }
    }

    //策略2：使用软件自带的工具(位于程序运行目录或项目根目录的bundledSubpath下)
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;
    searchPaths << QDir(appDir).filePath(bundledSubpath);

    //开发环境适配：exe在build/子目录中，向上回溯查找项目根目录
    QDir currentDir(appDir);
    for (int i = 0; i < 4; ++i) {
        if (!currentDir.cdUp()) break;
        QString candidate = currentDir.filePath(bundledSubpath);
        if (!searchPaths.contains(candidate)) {
            searchPaths << candidate;
        }
    }

    //遍历候选路径，使用第一个存在的
    for (const QString &candidate : searchPaths) {
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isExecutable()) {
            Logger::instance()->debug(logTag, QString("✅ 使用软件自带%1: %2").arg(toolName).arg(candidate));
            return candidate;
        }
    }

    Logger::instance()->critical(logTag,
        QString("❌ 未找到%1环境！搜索路径: %2").arg(toolName).arg(searchPaths.join(" / ")));
    return QString();
}
