#include "VideoPlayback_Weight.h"
#include "ui_VideoPlayback_Weight.h"
#include "VideoPlayerWidget.h"
#include "WebSearchSidebar.h"
#include "Net_Module/BiliApiWorker.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include <QStandardPaths>
#include <QDir>
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDateTime>
#include <QFile>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>

VideoPlayback_Weight::VideoPlayback_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayback_Weight)
    , m_apiWorker(new BiliApiWorker(this))
    , m_ffmpeg(new FFmpeg_module(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("预览 — 双屏播放对比"));
    resize(1200, 700);

    //设置播放器标签(.ui中的提升控件已由setupUi创建)
    ui->localPlayer->setLabel(QStringLiteral("  本地缓存"));
    ui->onlinePlayer->setLabel(QStringLiteral("  在线对比"));

    //侧边栏关联API工(Work)er
    ui->sidebar->setApiWorker(m_apiWorker);

    //设置三栏比例 (左:中:右 = 3:3:2)
    ui->splitter->setStretchFactor(0, 3);
    ui->splitter->setStretchFactor(1, 3);
    ui->splitter->setStretchFactor(2, 2);

    //=== 信号槽 ===
    connect(m_ffmpeg, &FFmpeg_module::finished, this, &VideoPlayback_Weight::onLocalMuxFinished);
    connect(ui->sidebar, &WebSearchSidebar::resultSelected, this, &VideoPlayback_Weight::onOnlineResultSelected);
    connect(ui->sidebar, &WebSearchSidebar::availabilityUpdated, this, &VideoPlayback_Weight::onAvailabilityUpdated);
    connect(m_apiWorker, &BiliApiWorker::playUrlReady, this, &VideoPlayback_Weight::onPlayUrlReady);

    Logger::instance()->debug("VideoPlayback", "VideoPlayback_Weight 初始化完成");
}

VideoPlayback_Weight::~VideoPlayback_Weight()
{
    cleanupTempFile();
    delete ui;
}

void VideoPlayback_Weight::loadCacheData(const ParsedCacheData &data)
{
    m_currentData = data;

    //设置窗口标题
    setWindowTitle(QStringLiteral("预览 — %1").arg(data.videoInfo.title));

    //启动本地混流
    startLocalMux();

    //启动下架检测(如果有BV号)
    m_currentBvid = data.videoInfo.bvid;
    if (!m_currentBvid.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  正在检测下架状态..."));
        m_apiWorker->checkAvailability(m_currentBvid);

        //同时预填搜索框并自动搜索
        ui->sidebar->setSearchText(m_currentBvid);
    } else {
        ui->statusBar->setText(QStringLiteral("  无BV号，跳过下架检测"));
    }
}

void VideoPlayback_Weight::startLocalMux()
{
    if (!m_currentData.videoInfo.isValid()) {
        ui->statusBar->setText(QStringLiteral("  ❌ 缓存数据无效(音视频路径缺失)"));
        return;
    }

    //生成临时文件路径
    m_tempFilePath = generateTempPath();

    //构建混流请求(流拷贝MP4)
    MuxRequest req;
    req.videoPath = m_currentData.videoInfo.videoFilePath;
    req.audioPath = m_currentData.videoInfo.audioFilePath;
    req.outputPath = m_tempFilePath;
    req.format = OutputFormat::MP4;

    ui->statusBar->setText(QStringLiteral("  正在混流本地缓存..."));

    //启动FFmpeg混流
    m_ffmpeg->startMux(req);

    Logger::instance()->debug("VideoPlayback",
        QString("启动本地混流: %1 → %2").arg(m_currentData.videoInfo.title, m_tempFilePath));
}

void VideoPlayback_Weight::onLocalMuxFinished(bool success, const QString &message)
{
    if (success) {
        //加载临时文件到本地播放器
        ui->localPlayer->loadFile(m_tempFilePath);
        ui->statusBar->setText(QStringLiteral("  ✅ 本地缓存已就绪"));
        Logger::instance()->debug("VideoPlayback", "本地混流完成，播放器已加载");
    } else {
        ui->statusBar->setText(QStringLiteral("  ❌ 本地混流失败: %1").arg(message));
        Logger::instance()->critical("VideoPlayback",
            QString("本地混流失败: %1").arg(message));
        cleanupTempFile();
    }
}

void VideoPlayback_Weight::onOnlineResultSelected(const BiliSearchResult &result)
{
    ui->statusBar->setText(QStringLiteral("  正在获取在线播放地址..."));

    //先通过view接口获取cid
    QNetworkAccessManager *nam = m_apiWorker->findChild<QNetworkAccessManager*>();
    if (!nam)
        return;

    QUrl url("https://api.bilibili.com/x/web-interface/view");
    QUrlQuery query;
    query.addQueryItem("bvid", result.bvid);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    request.setRawHeader("Referer", "https://www.bilibili.com");
    if (m_apiWorker->isLoggedIn())
        request.setRawHeader("Cookie", m_apiWorker->cookie().toUtf8());

    QNetworkReply *reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, result]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            ui->statusBar->setText(QStringLiteral("  ❌ 获取视频信息失败"));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonObject data = root.value("data").toObject();
        qint64 cid = data.value("cid").toVariant().toLongLong();

        if (cid > 0) {
            //获取播放地址
            m_apiWorker->fetchPlayUrl(result.bvid, static_cast<int>(cid));
            m_currentBvid = result.bvid;
        } else {
            ui->statusBar->setText(QStringLiteral("  ❌ 无法获取视频CID"));
        }
    });
}

void VideoPlayback_Weight::onPlayUrlReady(const QString &bvid, const QString &playUrl)
{
    Q_UNUSED(bvid)
    //加载在线播放地址
    ui->onlinePlayer->loadFile(playUrl);
    ui->statusBar->setText(QStringLiteral("  ✅ 在线视频已加载"));
    Logger::instance()->debug("VideoPlayback",
        QString("在线播放地址已加载: %1").arg(playUrl.left(80)));
}

void VideoPlayback_Weight::onAvailabilityUpdated(bool isAvailable, const QString &description)
{
    QString status = isAvailable
        ? QStringLiteral("  ✅ 下架检测: %1").arg(description)
        : QStringLiteral("  ❌ 下架检测: %1").arg(description);
    ui->statusBar->setText(status);
}

QString VideoPlayback_Weight::generateTempPath() const
{
    //临时文件放在AppLocalData/preview_cache目录
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString previewDir = baseDir + "/preview_cache";
    QDir().mkpath(previewDir);

    //文件名: preview_<avid>_<timestamp>.mp4
    QString fileName = QStringLiteral("preview_%1_%2.mp4")
        .arg(m_currentData.videoInfo.avid)
        .arg(QDateTime::currentDateTime().toSecsSinceEpoch());

    return QDir(previewDir).filePath(fileName);
}

void VideoPlayback_Weight::cleanupTempFile()
{
    if (!m_tempFilePath.isEmpty()) {
        QFile file(m_tempFilePath);
        if (file.exists()) {
            if (file.remove()) {
                Logger::instance()->debug("VideoPlayback",
                    QString("已删除临时文件: %1").arg(m_tempFilePath));
            }
        }
        m_tempFilePath.clear();
    }
}

void VideoPlayback_Weight::closeEvent(QCloseEvent *event)
{
    //停止播放器
    ui->localPlayer->stop();
    ui->onlinePlayer->stop();

    //清理临时文件
    cleanupTempFile();

    //停止FFmpeg(如果还在运行)
    m_ffmpeg->stopMux();

    Logger::instance()->debug("VideoPlayback", "预览窗口已关闭，临时文件已清理");
    QWidget::closeEvent(event);
}
