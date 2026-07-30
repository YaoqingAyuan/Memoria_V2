#include "FFmpeg_module.h"
#include "logger.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QProcessEnvironment>

//FFmpeg模块类

/* ============================================================
 *  FFmpeg_module 工作流（对应架构图5步骤）
 * ============================================================
 *
 *  |  selfCheck()      |  <-- 1.自检验证：检测FFmpeg环境
 *           ↓
 *  |  startMux()       |  <-- 3.混流进程启动：接收MuxRequest
 *           ↓
 *  |  buildCommand()   |  <-- 2.构建指令：按格式分支(MP4/MKV/MOV/WEBM)
 *           ↓
 *  | Logger打印完整指令 |  <-- 设计原则：执行前先打印指令，便于定位错误
 *           ↓
 *  | QProcess异步执行   |  <-- 4.执行：不阻塞UI线程
 *           ↓
 *  | onReadyReadStderr |  <-- 解析进度(time=) → 发射progressUpdated信号
 *           ↓
 *  | onFinished()      |  <-- 5.完成：发射finished信号
 * ============================================================
 */

// ========== TranscodeParams 实现 ==========

//将转码参数序列化为FFmpeg命令行参数列表(WEBM/VP9专用)
QStringList TranscodeParams::toArgs() const {
    QStringList args;
    args << "-c:v" << "libvpx-vp9";     //视频编码器：VP9
    args << "-crf" << QString::number(crf); //恒定质量模式
    args << "-b:v" << "0";              //启用CRF模式(必须设-b:v 0)
    args << "-cpu-used" << QString::number(cpuUsed); //速度/质量权衡
    args << "-c:a" << "libopus";        //音频编码器：Opus
    args << "-b:a" << QString::number(audioBitrate) + "k"; //音频码率
    args << "-application" << "audio";  //Opus针对语音/音乐优化
    args << "-threads" << QString::number(threads); //编码线程数
    return args;
}

// ========== FFmpeg_module 构造/析构 ==========

FFmpeg_module::FFmpeg_module(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_ffmpegPath()
    , m_totalDuration(0.0)
{
    //连接QProcess信号槽
    connect(m_process, &QProcess::readyReadStandardError,
            this, &FFmpeg_module::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FFmpeg_module::onFinished);

    //初始化正则表达式：匹配 Duration: HH:MM:SS.xx 和 time=HH:MM:SS.xx
    m_durationRegex.setPattern("Duration:\\s*(\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{2})");
    m_timeRegex.setPattern("time=(\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{2})");

    Logger::instance()->debug("FFmpeg", "FFmpeg_module 实例已创建");
}

FFmpeg_module::~FFmpeg_module() {
    if (m_process->state() == QProcess::Running) {
        Logger::instance()->warning("FFmpeg", "析构时进程仍在运行，强制终止");
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    Logger::instance()->debug("FFmpeg", "FFmpeg_module 实例已销毁");
}

// ========== 1. 自检验证(初始化事件) ==========

//检测FFmpeg环境：优先使用用户设备已存在的环境，否则调用软件自带环境
QString FFmpeg_module::selfCheck() {
    Logger::instance()->debug("FFmpeg", ">>> 开始自检验证：检测FFmpeg环境");

    //策略1：检测系统PATH环境变量中是否有ffmpeg
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString pathEnv = env.value("PATH");
    Logger::instance()->debug("FFmpeg", QString("系统PATH长度: %1 字符").arg(pathEnv.length()));

    //尝试用"where ffmpeg"(Windows)查找用户环境
    QProcess probe;
    probe.start("where", QStringList() << "ffmpeg");
    bool found = probe.waitForFinished(3000);
    if (found && probe.exitCode() == 0) {
        QString result = QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();
        if (!result.isEmpty() && !result.contains("INFO: Could not find files")) {
            m_ffmpegPath = result.split('\n').first().trimmed();
            Logger::instance()->debug("FFmpeg", QString("✅ 检测到用户环境FFmpeg: %1").arg(m_ffmpegPath));
            return m_ffmpegPath;
        }
    }

    //策略2：使用软件自带的FFmpeg(位于程序运行目录或项目根目录的FFmpeg_tools/bin/下)
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;
    searchPaths << QDir(appDir).filePath("FFmpeg_tools/bin/ffmpeg.exe");

    //开发环境适配：exe在build/.../子目录中，向上回溯查找项目根目录下的FFmpeg_tools
    QDir currentDir(appDir);
    for (int i = 0; i < 4; ++i) {
        if (!currentDir.cdUp()) break;
        QString candidate = currentDir.filePath("FFmpeg_tools/bin/ffmpeg.exe");
        if (!searchPaths.contains(candidate)) {
            searchPaths << candidate;
        }
    }

    //遍历候选路径，使用第一个存在的
    bool foundBundled = false;
    for (const QString &candidate : searchPaths) {
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isExecutable()) {
            m_ffmpegPath = candidate;
            Logger::instance()->debug("FFmpeg", QString("✅ 使用软件自带FFmpeg: %1").arg(m_ffmpegPath));
            foundBundled = true;
            break;
        }
    }

    if (!foundBundled) {
        Logger::instance()->critical("FFmpeg",
            QString("❌ 致命错误：未找到FFmpeg环境！自带路径不存在: %1").arg(searchPaths.join(" / ")));
        m_ffmpegPath.clear();
    }

    return m_ffmpegPath;
}

// ========== 2. 构建指令 ==========

//根据MuxRequest构建完整的FFmpeg命令行参数列表
QStringList FFmpeg_module::buildCommand(const MuxRequest &request) {
    QStringList args;

    switch (request.format) {
    case OutputFormat::MP4:
        args = buildCopyCommand(request, "mp4");
        //MP4特有：添加+faststart优化网络播放
        args << "-movflags" << "+faststart";
        break;
    case OutputFormat::MKV:
        args = buildCopyCommand(request, "mkv");
        break;
    case OutputFormat::MOV:
        args = buildCopyCommand(request, "mov");
        break;
    case OutputFormat::WEBM:
        args = buildWebmCommand(request);
        break;
    }

    return args;
}

//复制流指令构建(MP4/MKV/MOV通用)
//-map 0:v:0 -map 1:a:0 -c copy
QStringList FFmpeg_module::buildCopyCommand(const MuxRequest &request, const QString &formatExt) {
    QStringList args;
    args << "-y";                            //覆盖输出文件不询问
    args << "-i" << request.videoPath;       //输入：视频流
    args << "-i" << request.audioPath;       //输入：音频流
    args << "-map" << "0:v:0";               //映射第1个输入的视频流
    args << "-map" << "1:a:0";               //映射第2个输入的音频流
    args << "-c" << "copy";                  //复制流，不转码
    //输出路径已含扩展名，formatExt仅用于日志记录
    Q_UNUSED(formatExt)
    args << request.outputPath;
    return args;
}

//WEBM转码指令构建(VP9+Opus)
QStringList FFmpeg_module::buildWebmCommand(const MuxRequest &request) {
    QStringList args;
    args << "-y";                            //覆盖输出文件不询问
    args << "-i" << request.videoPath;       //输入：视频流
    args << "-i" << request.audioPath;       //输入：音频流
    args << "-map" << "0:v:0";               //映射第1个输入的视频流
    args << "-map" << "1:a:0";               //映射第2个输入的音频流
    args << request.params.toArgs();         //追加WEBM转码参数
    args << request.outputPath;
    return args;
}

// ========== 3. 混流进程启动 ==========

//核心入口：接收请求，构建指令，打印日志，异步启动QProcess
void FFmpeg_module::startMux(const MuxRequest &request) {
    Logger::instance()->debug("FFmpeg", ">>> 开始混流任务");

    //检查是否已有进程在运行
    if (m_process->state() == QProcess::Running) {
        Logger::instance()->warning("FFmpeg", "已有混流进程在运行，拒绝重复启动");
        emit finished(false, "已有任务正在运行，请等待完成或停止后再试");
        return;
    }

    //输入验证：检查路径非空
    if (request.videoPath.isEmpty() || request.audioPath.isEmpty()) {
        Logger::instance()->critical("FFmpeg", "❌ 输入路径为空，无法启动混流");
        emit finished(false, "输入文件路径为空");
        return;
    }
    if (request.outputPath.isEmpty()) {
        Logger::instance()->critical("FFmpeg", "❌ 输出路径为空，无法启动混流");
        emit finished(false, "输出文件路径为空");
        return;
    }

    //自检验证：确保FFmpeg路径已确定
    if (m_ffmpegPath.isEmpty()) {
        Logger::instance()->warning("FFmpeg", "FFmpeg路径为空，执行自检验证");
        selfCheck();
        if (m_ffmpegPath.isEmpty()) {
            Logger::instance()->critical("FFmpeg", "❌ 自检验证失败：未找到FFmpeg环境");
            emit finished(false, "未找到FFmpeg环境，无法启动混流");
            return;
        }
    }

    //构建FFmpeg指令
    QStringList arguments = buildCommand(request);

    //★设计原则：在开始混流进程之前，将完整指令打印到控制台
    //便于后续出错时，方便定位错误位置(可复制到CMD手动测试)
    QString formatName;
    switch (request.format) {
    case OutputFormat::MP4:  formatName = "MP4(复制流)"; break;
    case OutputFormat::MKV:  formatName = "MKV(复制流)"; break;
    case OutputFormat::MOV:  formatName = "MOV(复制流)"; break;
    case OutputFormat::WEBM: formatName = "WEBM(转码)"; break;
    }
    Logger::instance()->debug("FFmpeg", QString("=== 混流指令 [%1] ===").arg(formatName));
    Logger::instance()->debug("FFmpeg", QString("执行路径: %1").arg(m_ffmpegPath));
    Logger::instance()->debug("FFmpeg", QString("完整参数: %1").arg(arguments.join(" ")));
    Logger::instance()->debug("FFmpeg", QString("输入视频: %1").arg(request.videoPath));
    Logger::instance()->debug("FFmpeg", QString("输入音频: %1").arg(request.audioPath));
    Logger::instance()->debug("FFmpeg", QString("输出文件: %1").arg(request.outputPath));
    Logger::instance()->debug("FFmpeg", "===============================");

    //重置进度状态
    m_totalDuration = 0.0;

    //启动QProcess(异步，不阻塞UI线程)
    m_process->start(m_ffmpegPath, arguments);

    if (!m_process->waitForStarted(5000)) {
        Logger::instance()->critical("FFmpeg",
            QString("❌ 进程启动失败！错误: %1").arg(m_process->errorString()));
        emit finished(false, QString("FFmpeg进程启动失败: %1").arg(m_process->errorString()));
        return;
    }

    Logger::instance()->debug("FFmpeg",
        QString("✅ 进程已启动，PID: %1").arg(m_process->processId()));
}

//停止当前混流进程
void FFmpeg_module::stopMux() {
    if (m_process->state() != QProcess::Running) {
        Logger::instance()->warning("FFmpeg", "停止请求：当前无运行中的进程");
        return;
    }

    Logger::instance()->debug("FFmpeg", ">>> 尝试优雅停止FFmpeg进程");
    m_process->terminate();  //尝试优雅退出

    //等待3秒，若未退出则强制终止
    if (!m_process->waitForFinished(3000)) {
        Logger::instance()->warning("FFmpeg", "优雅退出超时，强制终止进程");
        m_process->kill();
        m_process->waitForFinished(2000);
    }

    Logger::instance()->debug("FFmpeg", "✅ 进程已停止");
}

// ========== 4. 进度解析 ==========

//处理FFmpeg标准错误输出(进度信息通常在stderr中)
void FFmpeg_module::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromLocal8Bit(data);

    //发射原始日志信号(供UI日志面板显示)
    emit logOutput(output);

    //首次解析总时长(Duration: HH:MM:SS.xx)
    if (m_totalDuration <= 0) {
        m_totalDuration = parseDuration(output);
        if (m_totalDuration > 0) {
            Logger::instance()->debug("FFmpeg",
                QString("✅ 解析到视频总时长: %1 秒").arg(m_totalDuration, 0, 'f', 2));
        }
    }

    //解析当前处理时间(time=HH:MM:SS.xx)，计算进度百分比
    if (m_totalDuration > 0) {
        double currentTime = parseCurrentTime(output);
        if (currentTime > 0) {
            int percent = static_cast<int>((currentTime / m_totalDuration) * 100);
            if (percent >= 0 && percent <= 100) {
                emit progressUpdated(percent);
            }
        }
    }

    //捕获潜在错误信息
    if (output.contains("Error", Qt::CaseInsensitive) ||
        output.contains("Invalid", Qt::CaseInsensitive)) {
        Logger::instance()->warning("FFmpeg",
            QString("⚠️ 捕获到潜在错误: %1").arg(output.trimmed()));
    }
}

//进程结束处理
void FFmpeg_module::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Logger::instance()->debug("FFmpeg",
        QString(">>> 进程结束，退出码: %1, 状态: %2")
            .arg(exitCode)
            .arg(exitStatus == QProcess::NormalExit ? "正常退出" : "崩溃"));

    m_totalDuration = 0.0;  //重置

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        Logger::instance()->debug("FFmpeg", "✅ 混流任务完成！");
        emit finished(true, "混流完成");
    } else {
        QString errorMsg = QString("FFmpeg异常退出，退出码: %1").arg(exitCode);
        Logger::instance()->critical("FFmpeg", QString("❌ %1").arg(errorMsg));
        emit finished(false, errorMsg);
    }
}

// ========== 私有辅助函数 ==========

//从stderr文本中解析总时长(秒)
double FFmpeg_module::parseDuration(const QString &output) {
    QRegularExpressionMatch match = m_durationRegex.match(output);
    if (!match.hasMatch()) {
        return 0.0;
    }

    int h = match.captured(1).toInt();
    int m = match.captured(2).toInt();
    int s = match.captured(3).toInt();
    int cs = match.captured(4).toInt();  //百分秒
    return h * 3600 + m * 60 + s + cs / 100.0;
}

//从stderr文本中解析当前处理时间(秒)
double FFmpeg_module::parseCurrentTime(const QString &output) {
    QRegularExpressionMatch match = m_timeRegex.match(output);
    if (!match.hasMatch()) {
        return 0.0;
    }

    int h = match.captured(1).toInt();
    int m = match.captured(2).toInt();
    int s = match.captured(3).toInt();
    int cs = match.captured(4).toInt();
    return h * 3600 + m * 60 + s + cs / 100.0;
}
