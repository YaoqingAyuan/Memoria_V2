#include "AdbImportWorker.h"
#include "AdbModule.h"
#include "Parser_Module/CacheFileParser.h"
#include "core/logger.h"

#include <QFileInfo>

AdbImportWorker::AdbImportWorker(AdbModule *adb, QObject *parent)
    : QObject(parent)
    , m_adb(adb)
{
    //转发AdbModule的进度信号
    connect(m_adb, &AdbModule::pullProgressChanged, this, &AdbImportWorker::pullProgressChanged);
    connect(m_adb, &AdbModule::pullFinished, this, &AdbImportWorker::onPullFinished);
}

void AdbImportWorker::startPull(const QStringList &remotePaths, const QString &serial, const QString &localDir)
{
    m_pullQueue = remotePaths;
    m_pullTotal = remotePaths.size();
    m_pullCompleted = 0;
    m_localPullDir = localDir;
    m_currentSerial = serial;
    m_parsedData.clear();

    startNextPull();
}

QMap<QString, QList<ParsedCacheData>> AdbImportWorker::takeParsedData()
{
    return m_parsedData;
}

void AdbImportWorker::startNextPull()
{
    if (m_pullQueue.isEmpty()) {
        emit pullCompleted(m_pullCompleted, m_pullTotal);
        return;
    }

    QString remotePath = m_pullQueue.takeFirst();
    QString folderName = remotePath.section('/', -1);
    QString localPath = m_localPullDir + "/" + folderName;

    emit pullStarted(m_pullCompleted + 1, m_pullTotal, folderName);

    Logger::instance()->debug("AdbImportWorker",
        QStringLiteral("拉取中 (%1/%2): %3").arg(m_pullCompleted + 1).arg(m_pullTotal).arg(folderName));

    m_adb->pullFile(m_currentSerial, remotePath, localPath);
}

void AdbImportWorker::onPullFinished(const QString &localPath, bool success, const QString &message)
{
    m_pullCompleted++;

    QString folderName = QFileInfo(localPath).fileName();
    QList<ParsedCacheData> parsedList;

    if (success) {
        parsePulledFolder(localPath);
        parsedList = m_parsedData.value(folderName);
    } else {
        Logger::instance()->warning("AdbImportWorker", QString("拉取失败: %1").arg(message));
    }

    emit folderParsed(folderName, parsedList, localPath, success);

    startNextPull();
}

void AdbImportWorker::parsePulledFolder(const QString &localPath)
{
    CacheFileParser parser;
    QList<ParsedCacheData> parsedList;
    bool ok = parser.Cathe_Parse(localPath, parsedList);

    QString folderName = QFileInfo(localPath).fileName();

    if (ok && !parsedList.isEmpty()) {
        m_parsedData[folderName] = parsedList;
    }

    Q_UNUSED(localPath)
}
