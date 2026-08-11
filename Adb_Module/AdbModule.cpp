#include "AdbModule.h"
#include "logger.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

/* ============================================================
 *  AdbModule 工作流
 * ============================================================
 *
 *  |  selfCheck()       |  <-- 1.自检验证：检测ADB环境(用户adb → 随包adb)
 *           ↓
 *  |  refreshDevices()  |  <-- 2.设备发现：adb devices -l
 *           ↓
 *  |  pairDevice()      |  <-- 3a.无线配对：adb pair ip:port code
 *  |  connectDevice()   |  <-- 3b.无线连接：adb connect ip:port
 *           ↓
 *  |  listDir()         |  <-- 4.目录浏览：adb shell ls -la /remote/path
 *           ↓
 *  |  pullFile()        |  <-- 5.文件拉取：adb pull /remote/path /local/path
 *           ↓
 *  |  onFinished()      |  <-- 6.完成：根据Op类型路由解析结果→发射信号
 * ============================================================
 */

// ========== 构造/析构 ==========

AdbModule::AdbModule(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &AdbModule::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &AdbModule::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AdbModule::onFinished);

    //adb pull 进度格式: [  45%] /sdcard/file.ext
    m_pullProgressRegex.setPattern("\\[\\s*(\\d+)%\\]");

    //ls -la 行格式: drwxrwx--- 3 root root 4096 2024-01-15 10:30 filename
    //捕获组: 1=权限(10字符) 2=大小 3=日期+时间 4=文件名
    m_lsLineRegex.setPattern("^([drwxstp-]{10})\\s+\\d+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+(\\S+\\s+\\S+)\\s+(.+)$");

    Logger::instance()->debug("ADB", "AdbModule 实例已创建");
}

AdbModule::~AdbModule()
{
    if (m_process->state() == QProcess::Running) {
        Logger::instance()->warning("ADB", "析构时进程仍在运行，强制终止");
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    Logger::instance()->debug("ADB", "AdbModule 实例已销毁");
}

// ========== 1. 自检验证 ==========

QString AdbModule::selfCheck()
{
    Logger::instance()->debug("ADB", ">>> 开始自检验证：检测ADB环境");

    //策略1：检测系统PATH中是否有adb（Windows用where命令）
    QProcess probe;
    probe.start("where", QStringList() << "adb");
    bool found = probe.waitForFinished(3000);
    if (found && probe.exitCode() == 0) {
        QString result = QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();
        if (!result.isEmpty() && !result.contains("INFO: Could not find files")) {
            m_adbPath = result.split('\n').first().trimmed();
            Logger::instance()->debug("ADB", QString("✅ 检测到用户环境ADB: %1").arg(m_adbPath));
            return m_adbPath;
        }
    }

    //策略2：使用软件自带的ADB（位于 ADB_tools/bin/adb.exe）
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;
    searchPaths << QDir(appDir).filePath("ADB_tools/bin/adb.exe");

    //开发环境适配：exe在build/子目录中，向上回溯查找项目根目录下的ADB_tools
    QDir currentDir(appDir);
    for (int i = 0; i < 4; ++i) {
        if (!currentDir.cdUp()) break;
        QString candidate = currentDir.filePath("ADB_tools/bin/adb.exe");
        if (!searchPaths.contains(candidate)) {
            searchPaths << candidate;
        }
    }

    bool foundBundled = false;
    for (const QString &candidate : searchPaths) {
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isExecutable()) {
            m_adbPath = candidate;
            Logger::instance()->debug("ADB", QString("✅ 使用软件自带ADB: %1").arg(m_adbPath));
            foundBundled = true;
            break;
        }
    }

    if (!foundBundled) {
        Logger::instance()->critical("ADB",
            QString("❌ 未找到ADB环境！搜索路径: %1").arg(searchPaths.join(" / ")));
        m_adbPath.clear();
    }

    return m_adbPath;
}

// ========== 2. 设备管理 ==========

void AdbModule::refreshDevices()
{
    m_currentOp = Op::RefreshDevices;
    executeAdb({"devices", "-l"});
}

void AdbModule::pairDevice(const QString &ip, int port, const QString &code)
{
    m_currentOp = Op::Pair;
    QString addr = QString("%1:%2").arg(ip).arg(port);
    executeAdb({"pair", addr, code});
}

void AdbModule::connectDevice(const QString &ip, int port)
{
    m_currentOp = Op::Connect;
    QString addr = QString("%1:%2").arg(ip).arg(port);
    executeAdb({"connect", addr});
}

void AdbModule::disconnectDevice(const QString &serial)
{
    m_currentOp = Op::Disconnect;
    executeAdb({"disconnect", serial});
}

// ========== 3. 文件操作 ==========

void AdbModule::listDir(const QString &serial, const QString &remotePath)
{
    m_currentOp = Op::ListDir;
    m_opSerial = serial;
    m_opRemotePath = remotePath;
    executeAdb({"-s", serial, "shell", "ls", "-la", remotePath});
}

void AdbModule::pullFile(const QString &serial, const QString &remotePath, const QString &localPath)
{
    m_currentOp = Op::Pull;
    m_opSerial = serial;
    m_opRemotePath = remotePath;
    m_opLocalPath = localPath;
    executeAdb({"-s", serial, "pull", remotePath, localPath});
}

void AdbModule::stop()
{
    if (m_process->state() != QProcess::Running) {
        Logger::instance()->warning("ADB", "停止请求：当前无运行中的进程");
        return;
    }

    Logger::instance()->debug("ADB", ">>> 尝试停止ADB进程");
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        Logger::instance()->warning("ADB", "优雅退出超时，强制终止");
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

// ========== 私有：执行adb命令 ==========

void AdbModule::executeAdb(const QStringList &args)
{
    if (m_process->state() == QProcess::Running) {
        Logger::instance()->warning("ADB", "已有操作在运行，拒绝重复启动");
        emit errorOccurred("已有ADB操作在运行，请等待完成");
        return;
    }
    if (m_adbPath.isEmpty()) {
        Logger::instance()->critical("ADB", "ADB路径为空，请先执行selfCheck");
        emit errorOccurred("ADB路径为空，请先执行selfCheck");
        return;
    }

    //★设计原则：执行前先打印完整命令（与FFmpeg_module一致，便于错误定位）
    Logger::instance()->debug("ADB", QString("执行: %1 %2").arg(m_adbPath, args.join(" ")));

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_process->start(m_adbPath, args);

    if (!m_process->waitForStarted(5000)) {
        Logger::instance()->critical("ADB",
            QString("❌ 进程启动失败: %1").arg(m_process->errorString()));
        emit errorOccurred(QString("ADB进程启动失败: %1").arg(m_process->errorString()));
        m_currentOp = Op::Idle;
    }
}

// ========== 私有：输出处理 ==========

void AdbModule::onReadyReadStandardOutput()
{
    m_stdoutBuffer += m_process->readAllStandardOutput();
}

void AdbModule::onReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    m_stderrBuffer += data;

    //adb pull 的进度百分比在 stderr 中，格式: [  45%] /path/to/file
    if (m_currentOp == Op::Pull) {
        QString output = QString::fromUtf8(data);
        if (output.contains(QChar::ReplacementCharacter))
            output = QString::fromLocal8Bit(data);

        //取最后一个百分比（一次stderr可能包含多行进度）
        QRegularExpressionMatchIterator it = m_pullProgressRegex.globalMatch(output);
        int lastPercent = -1;
        while (it.hasNext()) {
            lastPercent = it.next().captured(1).toInt();
        }
        if (lastPercent >= 0) {
            emit pullProgressChanged(lastPercent);
        }
    }
}

void AdbModule::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    //解码输出（UTF-8优先，回退本地编码——与FFmpeg_module一致）
    QString stdoutOutput = QString::fromUtf8(m_stdoutBuffer);
    if (stdoutOutput.contains(QChar::ReplacementCharacter))
        stdoutOutput = QString::fromLocal8Bit(m_stdoutBuffer);

    QString stderrOutput = QString::fromUtf8(m_stderrBuffer);
    if (stderrOutput.contains(QChar::ReplacementCharacter))
        stderrOutput = QString::fromLocal8Bit(m_stderrBuffer);

    Logger::instance()->debug("ADB", QString("进程结束, 退出码: %1, 状态: %2")
        .arg(exitCode)
        .arg(exitStatus == QProcess::NormalExit ? "正常" : "崩溃"));

    Op finishedOp = m_currentOp;
    m_currentOp = Op::Idle;

    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

    switch (finishedOp) {
    case Op::RefreshDevices:
        if (success) {
            auto devices = parseDevices(stdoutOutput);
            Logger::instance()->debug("ADB",
                QString("✅ 发现 %1 台设备").arg(devices.size()));
            emit deviceListChanged(devices);
        } else {
            emit errorOccurred(QString("adb devices 失败: %1").arg(stderrOutput));
        }
        break;

    case Op::Pair:
        //adb pair 成功时输出 "Successfully paired to ..."
        emit pairResult(success, success ? stdoutOutput.trimmed() : stderrOutput.trimmed());
        break;

    case Op::Connect:
        //adb connect 成功时输出 "connected to ..."
        emit connectResult(success, success ? stdoutOutput.trimmed() : stderrOutput.trimmed());
        break;

    case Op::Disconnect:
        if (success) {
            Logger::instance()->debug("ADB", "✅ 设备已断开");
            refreshDevices();   //断开后刷新设备列表
        } else {
            emit errorOccurred(QString("adb disconnect 失败: %1").arg(stderrOutput));
        }
        break;

    case Op::ListDir:
        if (success) {
            auto entries = parseDirListing(stdoutOutput);
            Logger::instance()->debug("ADB",
                QString("✅ 目录 %1 解析到 %2 个条目").arg(m_opRemotePath).arg(entries.size()));
            emit dirListReady(m_opRemotePath, entries);
        } else {
            emit errorOccurred(QString("adb shell ls 失败: %1").arg(stderrOutput));
        }
        break;

    case Op::Pull:
        if (success) {
            emit pullProgressChanged(100);
            Logger::instance()->debug("ADB",
                QString("✅ 拉取完成: %1 → %2").arg(m_opRemotePath, m_opLocalPath));
            emit pullFinished(m_opLocalPath, true, stdoutOutput.trimmed());
        } else {
            Logger::instance()->critical("ADB",
                QString("❌ 拉取失败: %1").arg(stderrOutput));
            emit pullFinished(m_opLocalPath, false, stderrOutput.trimmed());
        }
        break;

    case Op::Idle:
        break;
    }
}

// ========== 私有：输出解析 ==========

QList<AdbDeviceInfo> AdbModule::parseDevices(const QString &output)
{
    QList<AdbDeviceInfo> devices;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        //跳过 "List of devices attached" 头行
        if (trimmed.startsWith("List of") || trimmed.isEmpty())
            continue;

        //格式: serial    state    product:xxx model:xxx device:xxx transport_id:N
        QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2)
            continue;

        AdbDeviceInfo info;
        info.serial = parts[0];
        info.state = parts[1];

        //从剩余字段提取 model:xxx
        for (int i = 2; i < parts.size(); ++i) {
            if (parts[i].startsWith("model:")) {
                info.model = parts[i].mid(6);      //去掉 "model:" 前缀
                info.model.replace('_', ' ');       //Pixel_7_Pro → Pixel 7 Pro
                break;
            }
        }

        devices.append(info);
    }

    return devices;
}

QList<AdbDirEntry> AdbModule::parseDirListing(const QString &output)
{
    QList<AdbDirEntry> entries;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        //跳过 "total NNN" 汇总行
        if (trimmed.startsWith("total ") || trimmed.isEmpty())
            continue;

        //使用正则匹配 ls -la 标准格式
        QRegularExpressionMatch match = m_lsLineRegex.match(trimmed);
        if (!match.hasMatch())
            continue;

        AdbDirEntry entry;
        QString permissions = match.captured(1);
        entry.isDir = permissions.startsWith('d');
        entry.size = match.captured(2).toLongLong();
        entry.date = match.captured(3);
        entry.name = match.captured(4).trimmed();

        //跳过 "." 和 ".."
        if (entry.name == "." || entry.name == "..")
            continue;

        entries.append(entry);
    }

    return entries;
}
