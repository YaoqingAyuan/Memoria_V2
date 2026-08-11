#include "mainwindow.h"
#include "Parser_Module/CacheFileParser.h"
#include "Core/ParsedCacheData.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "ADB_Module/AdbModule.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QRegularExpression>

// ============================================================================
// 【临时ADB自检测试】测试完毕后，注释掉下面这行宏定义即可恢复原主流程
// 测试目标：验证 AdbModule::selfCheck() 能否找到 ADB_tools/bin/adb.exe
// 验证项：1.selfCheck路径解析  2.adb version 可执行
// ============================================================================
//#define TEST_ADB_SELFCHECK

// ============================================================================
// 【临时闭环测试】测试完毕后，注释掉下面这行宏定义即可恢复原主流程
// 测试目标：验证 CacheFileParser + ParsedCacheData + FFmpeg_module 三模块协作
// 输入：D:/RawCacheFile_TestData/116451998503195
// 输出：D:/Memoria_OutputTest/<视频标题>.mp4(标题取自 VideoInfo.title)
// 注：日志通过 Logger(qDebug) 输出，Qt Creator 中查看"应用程序输出"面板
// ============================================================================
//#define TEST_CLOSED_LOOP

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef TEST_ADB_SELFCHECK
    // ==================== ADB自检测试：开始 ====================
    Logger::instance()->debug("ADB_TEST", "========== ADB自检测试开始 ==========");

    AdbModule adb;
    QString adbPath = adb.selfCheck();

    if (adbPath.isEmpty()) {
        Logger::instance()->critical("ADB_TEST", "❌ ADB自检失败：未找到adb.exe");
        return -1;
    }

    Logger::instance()->debug("ADB_TEST", QString("✅ ADB自检成功，路径: %1").arg(adbPath));

    //额外验证：执行 adb version 确认adb可正常运行（非selfCheck的一部分，仅辅助验证）
    QProcess versionProbe;
    versionProbe.start(adbPath, QStringList() << "version");
    if (versionProbe.waitForFinished(5000) && versionProbe.exitCode() == 0) {
        QString version = QString::fromUtf8(versionProbe.readAllStandardOutput()).trimmed();
        Logger::instance()->debug("ADB_TEST", QString("✅ adb version 输出:\n%1").arg(version));
    } else {
        Logger::instance()->warning("ADB_TEST", "⚠️ adb version 执行失败（不影响selfCheck验证结果）");
    }

    Logger::instance()->debug("ADB_TEST", "========== ADB自检测试结束 ==========");
    return 0;
    // ==================== ADB自检测试：结束 ====================

#elif defined(TEST_CLOSED_LOOP)
    // ==================== 临时闭环测试：开始 ====================
    Logger::instance()->debug("TEST", "========== 闭环测试开始 ==========");

    // 硬编码测试输入输出路径(临时测试，后续注释掉整个 #ifdef 区块即可)
    const QString testInputPath = "D:/RawCacheFile_TestData/116451998503195";
    const QString testOutputDir = "D:/Memoria_OutputTest";

    // 创建输出目录(若不存在)
    QDir outDir(testOutputDir);
    if (!outDir.exists() && !outDir.mkpath(testOutputDir)) {
        Logger::instance()->critical("TEST", QString("❌ 无法创建输出目录: %1").arg(testOutputDir));
        return -1;
    }

    // ---------- 1. 缓存解析(CacheFileParser → ParsedCacheData) ----------
    CacheFileParser parser;
    ParsedCacheData parsedData;
    bool parseOk = parser.Cathe_Parse(testInputPath, parsedData);

    Logger::instance()->debug("TEST", QString("解析结果: %1").arg(parseOk ? "✅ 成功" : "❌ 失败"));
    Logger::instance()->debug("TEST", QString("视频标题(title): %1").arg(parsedData.videoInfo.title));
    Logger::instance()->debug("TEST", QString("AVID=%1, BVID=%2").arg(parsedData.videoInfo.avid).arg(parsedData.videoInfo.bvid));
    Logger::instance()->debug("TEST", QString("Up主: %1").arg(parsedData.videoInfo.ownerName));
    Logger::instance()->debug("TEST", QString("视频文件: %1").arg(parsedData.videoInfo.videoFilePath));
    Logger::instance()->debug("TEST", QString("音频文件: %1").arg(parsedData.videoInfo.audioFilePath));
    Logger::instance()->debug("TEST", QString("视频流: %1x%2, 码率=%3").arg(parsedData.videoStream.width).arg(parsedData.videoStream.height).arg(parsedData.videoStream.bandwidth));
    Logger::instance()->debug("TEST", QString("isValid(): %1").arg(parsedData.videoInfo.isValid() ? "true" : "false"));

    if (!parseOk || !parsedData.videoInfo.isValid()) {
        Logger::instance()->critical("TEST", "❌ 解析失败或音视频路径无效，终止测试");
        return -1;
    }

    // ---------- 2. 构造输出文件名(标题取自 VideoInfo.title) ----------
    // 清理 Windows 文件名非法字符(统一使用 sanitizeFileName)
    const QString safeTitle = sanitizeFileName(
        parsedData.videoInfo.title,
        QString("untitled_%1").arg(parsedData.videoInfo.avid));
    QString outputPath = QDir(testOutputDir).filePath(safeTitle + ".mp4");
    Logger::instance()->debug("TEST", QString("输出文件: %1").arg(outputPath));

    // ---------- 3. FFmpeg自检 + 混流 ----------
    FFmpeg_module ffmpeg;
    QString ffmpegPath = ffmpeg.selfCheck();
    if (ffmpegPath.isEmpty()) {
        Logger::instance()->critical("TEST", "❌ FFmpeg自检失败，终止测试");
        return -1;
    }

    MuxRequest request;
    request.videoPath = parsedData.videoInfo.videoFilePath;
    request.audioPath = parsedData.videoInfo.audioFilePath;
    request.outputPath = outputPath;
    request.format = OutputFormat::MP4;  // MP4复制流，无需转码

    // 用事件循环等待异步混流完成信号(FFmpeg_module为异步QProcess架构)
    QEventLoop loop;
    bool muxSuccess = false;
    QString muxMsg;
    //用 QueuedConnection：startMux 在失败路径会同步发射 finished，入队后由 loop.exec() 处理，避免永久阻塞
    QObject::connect(&ffmpeg, &FFmpeg_module::finished, &loop,
                     [&loop, &muxSuccess, &muxMsg](bool success, const QString &msg) {
                         muxSuccess = success;
                         muxMsg = msg;
                         Logger::instance()->debug("TEST", QString("混流完成信号: success=%1, msg=%2").arg(success).arg(msg));
                         loop.quit();
                     }, Qt::QueuedConnection);
    QObject::connect(&ffmpeg, &FFmpeg_module::progressUpdated,
                     [](int percent) {
                         Logger::instance()->debug("TEST", QString("进度: %1%").arg(percent));
                     });
    QObject::connect(&ffmpeg, &FFmpeg_module::logOutput,
                     [](const QString &log) {
                         Logger::instance()->debug("FFmpeg", log.trimmed());
                     });

    ffmpeg.startMux(request);
    loop.exec();  // 阻塞等待 finished 信号

    // ---------- 4. 校验输出文件 ----------
    QFileInfo outInfo(outputPath);
    if (outInfo.exists() && outInfo.size() > 0) {
        Logger::instance()->debug("TEST", QString("✅ 输出文件已生成: %1").arg(outInfo.absoluteFilePath()));
        Logger::instance()->debug("TEST", QString("✅ 文件大小: %1 字节").arg(outInfo.size()));
    } else {
        Logger::instance()->critical("TEST", "❌ 输出文件未生成或为空");
    }

    Logger::instance()->debug("TEST", QString("========== 闭环测试结束 (混流%s) ==========").arg(muxSuccess ? "成功" : "失败"));
    return 0;
    // ==================== 临时闭环测试：结束 ====================

#else
    // ==================== 原主流程(闭环测试时跳过) ====================
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "Memoria_N_V2_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
#endif
}
