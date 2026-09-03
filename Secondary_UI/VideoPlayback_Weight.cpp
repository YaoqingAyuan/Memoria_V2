#include "VideoPlayback_Weight.h"
#include "ui_VideoPlayback_Weight.h"
#include "Net_Module/BiliApiWorker.h"
#include "Net_Module/HttpProxyServer.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include <QStyle>
#include <QVideoWidget>
#include <QSlider>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QCloseEvent>
#include <QFile>
#include <QDateTime>
#include <QPixmap>
#include <QMenu>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSettings>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QImage>
#include "ResultCardWidget.h"
#include "ThirdParty/qrcodegen.hpp"

VideoPlayback_Weight::VideoPlayback_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayback_Weight)
    , m_localPlayer(new QMediaPlayer(this))
    , m_localAudio(new QAudioOutput(this))
    , m_onlinePlayer(new QMediaPlayer(this))
    , m_onlineAudio(new QAudioOutput(this))
    , m_apiWorker(new BiliApiWorker(this))
    , m_ffmpeg(new FFmpeg_module(this))
    , m_proxy(new HttpProxyServer(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("预览 — 双屏播放对比"));
    //.ui文件中文编码损坏,用QStringLiteral覆盖所有中文文本
    ui->localPlayerTitle->setText(QStringLiteral("本地预览"));
    ui->onlinePlayerTitle->setText(QStringLiteral("在线预览"));
    ui->searchEdit->setPlaceholderText(QStringLiteral("标题/BV号搜索"));
    ui->searchBtn->setText(QStringLiteral("搜索"));
    ui->availabilityLabel->setText(QStringLiteral("下架状态："));
    ui->statusBar->setText(QStringLiteral("  就绪 — 等待加载数据"));
    resize(1200, 700);

    //布局stretch因子(不能在.ui中设置，UIC会生成错误的setStretch(QString)调用)
    //主布局: 播放器=1(独享剩余空间), 搜索栏=0(固定高), 结果列表=0(固定高), 状态栏=0(固定高)
    //窗口缩放时只有播放器区域大小变化，搜索结果栏高度不变
    ui->mainLayout->setStretch(0, 1);
    ui->mainLayout->setStretch(1, 0);
    ui->mainLayout->setStretch(2, 0);
    ui->mainLayout->setStretch(3, 0);
    //双播放器等宽
    ui->playersLayout->setStretch(0, 1);
    ui->playersLayout->setStretch(1, 1);
    //每个播放器内部: 标题=0, 视频区=1, 控制栏=0
    ui->localPlayerVLayout->setStretch(0, 0);
    ui->localPlayerVLayout->setStretch(1, 1);
    ui->localPlayerVLayout->setStretch(2, 0);
    ui->onlinePlayerVLayout->setStretch(0, 0);
    ui->onlinePlayerVLayout->setStretch(1, 1);
    ui->onlinePlayerVLayout->setStretch(2, 0);
    //控制栏内: 进度条(index=4)拉伸, 其余固定
    ui->localControlsLayout->setStretch(4, 1);
    ui->onlineControlsLayout->setStretch(4, 1);

    //搜索结果区: QScrollArea + QHBoxLayout(替代QListWidget IconMode)
    //固定高度: 卡片110px + 滚动条~20px + 上下边距
    ui->resultScrollArea->setFixedHeight(145);

    //封面图片下载管理器
    m_coverNam = new QNetworkAccessManager(this);

    //启动本地HTTP代理(注入Referer/Cookie绕过CDN防盗链)
    m_proxy->start();

    //播放引擎关联到 .ui 中的 QVideoWidget
    m_localPlayer->setVideoOutput(ui->localVideoWidget);
    m_localPlayer->setAudioOutput(m_localAudio);
    m_localAudio->setVolume(0.7f);

    m_onlinePlayer->setVideoOutput(ui->onlineVideoWidget);
    m_onlinePlayer->setAudioOutput(m_onlineAudio);
    m_onlineAudio->setVolume(0.7f);

    //音量滑块初始化
    ui->localVolumeSlider->setRange(0, 100);
    ui->localVolumeSlider->setValue(70);
    ui->onlineVolumeSlider->setRange(0, 100);
    ui->onlineVolumeSlider->setValue(70);

    initPlayerIcons();

    //登录按钮支持右键菜单
    ui->loginBtn->setContextMenuPolicy(Qt::CustomContextMenu);

    //=== 信号槽：本地播放器 ===
    connect(ui->localPlayBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onLocalPlayClicked);
    connect(ui->localRewindBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onLocalRewindClicked);
    connect(ui->localForwardBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onLocalForwardClicked);
    connect(m_localPlayer, &QMediaPlayer::positionChanged, this, &VideoPlayback_Weight::onLocalPositionChanged);
    connect(m_localPlayer, &QMediaPlayer::durationChanged, this, &VideoPlayback_Weight::onLocalDurationChanged);
    connect(ui->localProgressSlider, &QSlider::sliderMoved, this, &VideoPlayback_Weight::onLocalSliderMoved);
    connect(ui->localProgressSlider, &QSlider::sliderPressed, this, [this]() { m_localDragging = true; });
    connect(ui->localProgressSlider, &QSlider::sliderReleased, this, [this]() { m_localDragging = false; });
    connect(ui->localVolumeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_localAudio->setVolume(v / 100.0f);
    });
    connect(m_localPlayer, &QMediaPlayer::errorOccurred, this, &VideoPlayback_Weight::onLocalMediaError);
    connect(m_localPlayer, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayback_Weight::onLocalMediaStatusChanged);

    //=== 信号槽：云端播放器 ===
    connect(ui->onlinePlayBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onOnlinePlayClicked);
    connect(ui->onlineRewindBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onOnlineRewindClicked);
    connect(ui->onlineForwardBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onOnlineForwardClicked);
    connect(m_onlinePlayer, &QMediaPlayer::positionChanged, this, &VideoPlayback_Weight::onOnlinePositionChanged);
    connect(m_onlinePlayer, &QMediaPlayer::durationChanged, this, &VideoPlayback_Weight::onOnlineDurationChanged);
    connect(ui->onlineProgressSlider, &QSlider::sliderMoved, this, &VideoPlayback_Weight::onOnlineSliderMoved);
    connect(ui->onlineProgressSlider, &QSlider::sliderPressed, this, [this]() { m_onlineDragging = true; });
    connect(ui->onlineProgressSlider, &QSlider::sliderReleased, this, [this]() { m_onlineDragging = false; });
    connect(ui->onlineVolumeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_onlineAudio->setVolume(v / 100.0f);
    });
    connect(m_onlinePlayer, &QMediaPlayer::errorOccurred, this, &VideoPlayback_Weight::onOnlineMediaError);
    connect(m_onlinePlayer, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayback_Weight::onOnlineMediaStatusChanged);

    //=== 信号槽：FFmpeg混流 ===
    connect(m_ffmpeg, &FFmpeg_module::finished, this, &VideoPlayback_Weight::onLocalMuxFinished);

    //=== 信号槽：搜索 ===
    connect(ui->searchBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onSearchClicked);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &VideoPlayback_Weight::onSearchClicked);

    //=== 信号槽：BiliApiWorker ===
    connect(m_apiWorker, &BiliApiWorker::searchResultReady, this, &VideoPlayback_Weight::onSearchResultReady);
    connect(m_apiWorker, &BiliApiWorker::searchFailed, this, &VideoPlayback_Weight::onSearchFailed);
    connect(m_apiWorker, &BiliApiWorker::availabilityChecked, this, &VideoPlayback_Weight::onAvailabilityChecked);
    connect(m_apiWorker, &BiliApiWorker::playUrlReady, this, &VideoPlayback_Weight::onPlayUrlReady);
    connect(m_apiWorker, &BiliApiWorker::playUrlDashReady, this, &VideoPlayback_Weight::onPlayUrlDashReady);
    connect(m_apiWorker, &BiliApiWorker::playUrlFailed, this, &VideoPlayback_Weight::onPlayUrlFailed);
    connect(m_apiWorker, &BiliApiWorker::loginStatusChanged, this, &VideoPlayback_Weight::onLoginStatusChanged);

    //=== 信号槽：登录 ===
    connect(ui->loginBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onLoginBtnClicked);
    connect(ui->loginBtn, &QWidget::customContextMenuRequested, this, &VideoPlayback_Weight::onShowLoginContextMenu);

    updateLoginUI();

    Logger::instance()->debug("VideoPlayback", "VideoPlayback_Weight 初始化完成");
}

VideoPlayback_Weight::~VideoPlayback_Weight()
{
    cleanupTempFile();
    delete ui;
}

// ============================================================
// 播放器图标初始化
// ============================================================

void VideoPlayback_Weight::initPlayerIcons()
{
    auto *s = style();
    ui->localPlayBtn->setIcon(s->standardIcon(QStyle::SP_MediaPlay));
    ui->localRewindBtn->setIcon(s->standardIcon(QStyle::SP_MediaSkipBackward));
    ui->localForwardBtn->setIcon(s->standardIcon(QStyle::SP_MediaSkipForward));
    ui->localVolumeLabel->setPixmap(s->standardIcon(QStyle::SP_MediaVolume).pixmap(16, 16));

    ui->onlinePlayBtn->setIcon(s->standardIcon(QStyle::SP_MediaPlay));
    ui->onlineRewindBtn->setIcon(s->standardIcon(QStyle::SP_MediaSkipBackward));
    ui->onlineForwardBtn->setIcon(s->standardIcon(QStyle::SP_MediaSkipForward));
    ui->onlineVolumeLabel->setPixmap(s->standardIcon(QStyle::SP_MediaVolume).pixmap(16, 16));
}

void VideoPlayback_Weight::fetchCover(const QString &bvid, const QString &url)
{
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    QNetworkReply *reply = m_coverNam->get(request);
    reply->setProperty("bvid", bvid);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
        //按bvid查找卡片(搜索结果clear后旧请求自动失效)
        QString bvid = reply->property("bvid").toString();
        for (ResultCardWidget *card : m_resultCards) {
            if (card->bvid() == bvid) {
                QPixmap pixmap;
                pixmap.loadFromData(reply->readAll());
                card->setCover(pixmap);
                break;
            }
        }
    });
}

// ============================================================
// 加载缓存数据
// ============================================================

void VideoPlayback_Weight::loadCacheData(const ParsedCacheData &data)
{
    m_currentData = data;

    setWindowTitle(QStringLiteral("预览 — %1").arg(data.videoInfo.title));

    startLocalMux();

    m_currentBvid = data.videoInfo.bvid;
    //打开预览时自动搜索，优先级：BV号 > AV号 > 标题
    //BV号非空 → searchByBvid（同时触发下架检测）
    //BV号空但有AV号 → searchByAvid（view API支持aid参数）
    //都没有 → 用标题模糊搜索（清理文件后缀后搜索）
    if (!m_currentBvid.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  正在搜索并检测下架状态..."));
        ui->searchEdit->setText(m_currentBvid);
        m_apiWorker->searchByBvid(m_currentBvid);
    } else if (data.videoInfo.avid > 0) {
        ui->statusBar->setText(QStringLiteral("  正在按AV号搜索..."));
        ui->searchEdit->setText(QStringLiteral("av%1").arg(data.videoInfo.avid));
        m_apiWorker->searchByAvid(data.videoInfo.avid);
    } else if (!data.videoInfo.title.isEmpty()) {
        //清理文件后缀，避免 ".zip" 等干扰搜索
        QString cleanTitle = data.videoInfo.title;
        static const QStringList suffixes = {
            ".zip", ".rar", ".7z", ".mp4", ".mkv", ".mov", ".webm",
            ".ts", ".flv", ".avi", ".m4a", ".m4v"
        };
        for (const auto &suffix : suffixes) {
            if (cleanTitle.endsWith(suffix, Qt::CaseInsensitive)) {
                cleanTitle.chop(suffix.length());
                break;
            }
        }
        ui->statusBar->setText(QStringLiteral("  正在按标题搜索..."));
        ui->searchEdit->setText(cleanTitle);
        m_apiWorker->searchByKeyword(cleanTitle);
    } else {
        ui->statusBar->setText(QStringLiteral("  无BV号和标题，跳过搜索"));
    }
}

// ============================================================
// 本地播放器控制
// ============================================================

void VideoPlayback_Weight::onLocalPlayClicked()
{
    if (m_localPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_localPlayer->pause();
        ui->localPlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        m_localPlayer->play();
        ui->localPlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void VideoPlayback_Weight::onLocalRewindClicked()
{
    m_localPlayer->setPosition(m_localPlayer->position() - 5000);
}

void VideoPlayback_Weight::onLocalForwardClicked()
{
    m_localPlayer->setPosition(m_localPlayer->position() + 5000);
}

void VideoPlayback_Weight::onLocalPositionChanged(qint64 position)
{
    if (!m_localDragging)
        ui->localProgressSlider->setValue(static_cast<int>(position));
    ui->localTimeLabel->setText(
        QStringLiteral("%1 / %2").arg(formatTime(position), formatTime(m_localPlayer->duration())));
}

void VideoPlayback_Weight::onLocalDurationChanged(qint64 duration)
{
    ui->localProgressSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayback_Weight::onLocalSliderMoved(int position)
{
    m_localPlayer->setPosition(position);
}

void VideoPlayback_Weight::onLocalMediaError()
{
    QString error = m_localPlayer->errorString();
    if (error.isEmpty())
        error = QStringLiteral("未知播放错误");
    ui->statusBar->setText(QStringLiteral("  ❌ 本地播放错误: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("本地播放错误: %1").arg(error));
}

void VideoPlayback_Weight::onLocalMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::LoadedMedia) {
        ui->statusBar->setText(QStringLiteral("  ✅ 本地缓存已就绪"));
        Logger::instance()->debug("VideoPlayback", "本地媒体加载完成");
    }
}

// ============================================================
// 云端播放器控制
// ============================================================

void VideoPlayback_Weight::onOnlinePlayClicked()
{
    //若尚未加载有效视频且有搜索结果，自动播放首位视频
    QMediaPlayer::MediaStatus status = m_onlinePlayer->mediaStatus();
    if ((status == QMediaPlayer::NoMedia || status == QMediaPlayer::InvalidMedia)
            && !m_results.isEmpty()) {
        onOnlineResultSelected(m_results.first());
        return;
    }

    if (m_onlinePlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_onlinePlayer->pause();
        ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        m_onlinePlayer->play();
        ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void VideoPlayback_Weight::onOnlineRewindClicked()
{
    m_onlinePlayer->setPosition(m_onlinePlayer->position() - 5000);
}

void VideoPlayback_Weight::onOnlineForwardClicked()
{
    m_onlinePlayer->setPosition(m_onlinePlayer->position() + 5000);
}

void VideoPlayback_Weight::onOnlinePositionChanged(qint64 position)
{
    if (!m_onlineDragging)
        ui->onlineProgressSlider->setValue(static_cast<int>(position));
    ui->onlineTimeLabel->setText(
        QStringLiteral("%1 / %2").arg(formatTime(position), formatTime(m_onlinePlayer->duration())));
}

void VideoPlayback_Weight::onOnlineDurationChanged(qint64 duration)
{
    ui->onlineProgressSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayback_Weight::onOnlineSliderMoved(int position)
{
    m_onlinePlayer->setPosition(position);
}

void VideoPlayback_Weight::onOnlineMediaError()
{
    QString error = m_onlinePlayer->errorString();
    if (error.isEmpty())
        error = QStringLiteral("未知播放错误");
    ui->statusBar->setText(QStringLiteral("  ❌ 云端播放错误: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("云端播放错误: %1").arg(error));
}

void VideoPlayback_Weight::onOnlineMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::LoadedMedia) {
        ui->statusBar->setText(QStringLiteral("  ✅ 在线视频已加载"));
        Logger::instance()->debug("VideoPlayback", "云端媒体加载完成");
    }
}

// ============================================================
// 文件加载与播放器停止
// ============================================================

void VideoPlayback_Weight::loadLocalFile(const QString &path)
{
    m_localPlayer->setSource(QUrl::fromUserInput(path));
    ui->localTimeLabel->setText("00:00 / 00:00");
    ui->localProgressSlider->setValue(0);
}

void VideoPlayback_Weight::stopPlayers()
{
    m_localPlayer->stop();
    m_localPlayer->setSource(QUrl());
    m_onlinePlayer->stop();
    m_onlinePlayer->setSource(QUrl());

    ui->localTimeLabel->setText("00:00 / 00:00");
    ui->localProgressSlider->setValue(0);
    ui->localPlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

    ui->onlineTimeLabel->setText("00:00 / 00:00");
    ui->onlineProgressSlider->setValue(0);
    ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

// ============================================================
// FFmpeg混流
// ============================================================

void VideoPlayback_Weight::startLocalMux()
{
    if (!m_currentData.videoInfo.isValid()) {
        ui->statusBar->setText(QStringLiteral("  ❌ 缓存数据无效(音视频路径缺失)"));
        return;
    }

    m_tempFilePath = generateTempPath();

    MuxRequest req;
    req.videoPath = m_currentData.videoInfo.videoFilePath;
    req.audioPath = m_currentData.videoInfo.audioFilePath;
    req.outputPath = m_tempFilePath;
    req.format = OutputFormat::MP4;

    ui->statusBar->setText(QStringLiteral("  正在混流本地缓存..."));
    m_ffmpeg->startMux(req);

    Logger::instance()->debug("VideoPlayback",
        QString("启动本地混流: %1 → %2").arg(m_currentData.videoInfo.title, m_tempFilePath));
}

void VideoPlayback_Weight::onLocalMuxFinished(bool success, const QString &message)
{
    if (m_isOnlineMuxing) {
        //在线DASH混流完成
        m_isOnlineMuxing = false;
        if (success) {
            m_onlinePlayer->setSource(QUrl::fromLocalFile(m_onlineTempPath));
            m_onlinePlayer->play();
            ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
            ui->statusBar->setText(QStringLiteral("  正在播放在线视频(高清)..."));
            Logger::instance()->debug("VideoPlayback", "在线DASH混流完成，播放器已加载");
        } else {
            ui->statusBar->setText(QStringLiteral("  ❌ 在线混流失败: %1").arg(message));
            Logger::instance()->warning("VideoPlayback",
                QString("在线DASH混流失败: %1").arg(message));
            cleanupOnlineTempFile();
        }
    } else {
        //本地缓存混流完成
        if (success) {
            loadLocalFile(m_tempFilePath);
            Logger::instance()->debug("VideoPlayback", "本地混流完成，播放器已加载");
        } else {
            ui->statusBar->setText(QStringLiteral("  ❌ 本地混流失败: %1").arg(message));
            Logger::instance()->critical("VideoPlayback",
                QString("本地混流失败: %1").arg(message));
            cleanupTempFile();
        }
    }
}

// ============================================================
// 搜索
// ============================================================

void VideoPlayback_Weight::onSearchClicked()
{
    QString keyword = ui->searchEdit->text().trimmed();
    if (keyword.isEmpty())
        return;

    ui->statusBar->setText(QStringLiteral("  正在搜索..."));
    clearResultCards();
    m_results.clear();

    if (keyword.startsWith("BV", Qt::CaseInsensitive)) {
        m_apiWorker->searchByBvid(keyword);
    } else {
        m_apiWorker->searchByKeyword(keyword);
    }
}

void VideoPlayback_Weight::onSearchResultReady(const QList<BiliSearchResult> &results)
{
    m_results = results;
    clearResultCards();

    if (results.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  无搜索结果"));
        return;
    }

    for (const auto &r : results) {
        auto *card = new ResultCardWidget(r, ui->resultContainer);
        m_resultCards.append(card);
        ui->resultCardsLayout->addWidget(card);
        if (!r.coverUrl.isEmpty())
            fetchCover(r.bvid, r.coverUrl);
        connect(card, &ResultCardWidget::clicked, this, [this, r]() {
            onOnlineResultSelected(r);
        });
    }

    //末尾弹性空间, 让卡片左对齐
    ui->resultCardsLayout->addStretch();
    //更新容器最小宽度, 超出视口时自动出现水平滚动条
    ui->resultContainer->setMinimumWidth(results.size() * 124 + 8);

    ui->statusBar->setText(QStringLiteral("  找到 %1 条结果").arg(results.size()));
}

void VideoPlayback_Weight::onSearchFailed(const QString &error)
{
    ui->statusBar->setText(QStringLiteral("  ❌ 搜索失败: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("搜索失败: %1").arg(error));
}

void VideoPlayback_Weight::clearResultCards()
{
    while (ui->resultCardsLayout->count() > 0) {
        QLayoutItem *item = ui->resultCardsLayout->takeAt(0);
        if (QWidget *w = item->widget())
            delete w;
        delete item;
    }
    m_resultCards.clear();
}

// ============================================================
// 下架检测
// ============================================================

void VideoPlayback_Weight::onAvailabilityChecked(const QString &bvid, bool isAvailable, const QString &description)
{
    QString status = isAvailable
        ? QStringLiteral("✅ 在架: %1").arg(description)
        : QStringLiteral("❌ 已下架: %1").arg(description);
    ui->availabilityLabel->setText(status);
    ui->statusBar->setText(QStringLiteral("  下架检测: %1").arg(description));
}

// ============================================================
// 在线播放
// ============================================================

void VideoPlayback_Weight::onOnlineResultSelected(const BiliSearchResult &result)
{
    if (result.bvid.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  ❌ 搜索结果无BV号"));
        return;
    }
    if (result.cid <= 0) {
        ui->statusBar->setText(QStringLiteral("  ❌ 搜索结果无CID"));
        return;
    }

    ui->statusBar->setText(QStringLiteral("  正在获取在线播放地址..."));
    m_currentBvid = result.bvid;

    if (m_apiWorker->isLoggedIn()) {
        //已登录：DASH格式获取最高画质(1080P/4K)，FFmpeg混流后播放
        m_apiWorker->fetchPlayUrlDash(result.bvid, static_cast<int>(result.cid));
    } else {
        //未登录：MP4格式(480P)，通过代理即时播放
        m_apiWorker->fetchPlayUrl(result.bvid, static_cast<int>(result.cid), 32);
    }
}

void VideoPlayback_Weight::onPlayUrlReady(const QString &bvid, const QString &playUrl)
{
    //方案B: 通过本地HTTP代理注入Referer/Cookie头，绕过B站CDN防盗链
    //方案C(URL查询参数注入)因QUrl百分号编码导致Referer值失效，已弃用
    Logger::instance()->debug("VideoPlayback",
        QString("在线播放地址已获取，通过代理播放: %1").arg(playUrl.left(80)));

    cleanupOnlineTempFile();

    //设置代理的Referer和Cookie
    m_proxy->setReferer("https://www.bilibili.com");
    m_proxy->setCookie(QString());
    if (m_apiWorker->isLoggedIn()) {
        m_proxy->setCookie(m_apiWorker->cookie());
    }

    //通过代理URL播放(QMediaPlayer请求代理→代理注入头→转发CDN响应)
    QUrl proxyUrl = m_proxy->proxyUrl(QUrl(playUrl));
    m_onlinePlayer->setSource(proxyUrl);
    m_onlinePlayer->play();
    ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->statusBar->setText(QStringLiteral("  正在播放在线视频..."));
}

void VideoPlayback_Weight::onPlayUrlDashReady(const QString &bvid, const QString &videoUrl, const QString &audioUrl)
{
    //已登录高清播放：DASH分离的音视频URL → 本地代理注入头 → FFmpeg混流为临时MP4 → QMediaPlayer播放
    //通过代理而非FFmpeg -headers：Windows CommandLineToArgvW将\r\n视为空白，导致-headers值截断
    Logger::instance()->debug("VideoPlayback",
        QString("DASH播放地址已获取，通过代理混流: %1").arg(videoUrl.left(80)));

    cleanupOnlineTempFile();
    m_onlineTempPath = generateOnlineTempPath();

    //设置代理的Referer和Cookie(代理注入HTTP头，FFmpeg只需访问代理URL)
    m_proxy->setReferer("https://www.bilibili.com");
    m_proxy->setCookie(QString());
    if (m_apiWorker->isLoggedIn()) {
        m_proxy->setCookie(m_apiWorker->cookie());
    }

    //将CDN URL转为代理URL：http://127.0.0.1:port/proxy?url=ENCODED_CDN_URL
    QString proxyVideoUrl = m_proxy->proxyUrl(QUrl(videoUrl)).toString();
    QString proxyAudioUrl;
    if (!audioUrl.isEmpty()) {
        proxyAudioUrl = m_proxy->proxyUrl(QUrl(audioUrl)).toString();
    }

    MuxRequest req;
    req.videoPath = proxyVideoUrl;
    req.audioPath = proxyAudioUrl;
    req.outputPath = m_onlineTempPath;
    req.format = OutputFormat::MP4;

    m_isOnlineMuxing = true;
    ui->statusBar->setText(QStringLiteral("  正在缓冲在线视频(高清混流)..."));
    m_ffmpeg->startMux(req);

    Logger::instance()->debug("VideoPlayback",
        QString("启动在线DASH混流(代理) → %1").arg(m_onlineTempPath));
}
void VideoPlayback_Weight::onPlayUrlFailed(const QString &error)
{
    ui->statusBar->setText(QStringLiteral("  在线播放失败: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("在线播放地址获取失败: %1").arg(error));
}

// ============================================================
// B站登录
// ============================================================

void VideoPlayback_Weight::onLoginBtnClicked()
{
    if (!m_apiWorker || m_apiWorker->isLoggedIn())
        return;

    m_apiWorker->requestLoginQrCode();
    connect(m_apiWorker, &BiliApiWorker::qrCodeReady, this, [this](const QString &qrImageUrl, const QString &qrcodeKey) {
        m_qrcodeKey = qrcodeKey;
        showLoginDialog(qrImageUrl);
    }, Qt::SingleShotConnection);
}

void VideoPlayback_Weight::showLoginDialog(const QString &qrImageUrl)
{
    m_loginDialog = new QDialog(this);
    m_loginDialog->setWindowTitle(QStringLiteral("登录B站 — 扫码登录"));
    m_loginDialog->setFixedSize(300, 380);

    QVBoxLayout *layout = new QVBoxLayout(m_loginDialog);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(QStringLiteral("请使用B站APP扫码登录"), m_loginDialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(titleLabel);

    m_qrCodeLabel = new QLabel(m_loginDialog);
    m_qrCodeLabel->setAlignment(Qt::AlignCenter);
    m_qrCodeLabel->setMinimumSize(240, 240);
    m_qrCodeLabel->setStyleSheet("QLabel { background: white; border: 1px solid #ddd; }");
    layout->addWidget(m_qrCodeLabel);

    m_hintLabel = new QLabel(QStringLiteral("等待扫码..."), m_loginDialog);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(m_hintLabel);

    m_autoLoginCheck = new QCheckBox(QStringLiteral("自动登录(以后打开软件时保持登录)"), m_loginDialog);
    m_autoLoginCheck->setChecked(true);
    layout->addWidget(m_autoLoginCheck);

    //用Nayuki QR库将url字符串本地渲染为二维码(不依赖网络下载图片)
    try {
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
            qrImageUrl.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        int qrSize = qr.getSize();
        int scale = 8;  //每个模块8像素
        int margin = 4 * scale;  //4模块留白
        int imgSize = (qrSize + 8) * scale;
        QImage image(imgSize, imgSize, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                if (qr.getModule(x, y)) {
                    for (int dy = 0; dy < scale; dy++)
                        for (int dx = 0; dx < scale; dx++)
                            image.setPixel(margin + x * scale + dx, margin + y * scale + dy, 0xFF000000);
                }
            }
        }
        if (m_qrCodeLabel) {
            m_qrCodeLabel->setPixmap(QPixmap::fromImage(image).scaled(
                220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } catch (const std::exception &e) {
        if (m_hintLabel)
            m_hintLabel->setText(QStringLiteral("二维码生成失败"));
    }

    //启动轮询定时器
    m_loginPollTimer = new QTimer(this);
    connect(m_loginPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_qrcodeKey.isEmpty())
            m_apiWorker->pollLoginStatus(m_qrcodeKey);
    });
    m_loginPollTimer->start(2000);

    m_loginDialog->exec();

    if (m_loginPollTimer) {
        m_loginPollTimer->stop();
        m_loginPollTimer->deleteLater();
        m_loginPollTimer = nullptr;
    }
    delete m_loginDialog;
    m_loginDialog = nullptr;
    m_autoLoginCheck = nullptr;
}

void VideoPlayback_Weight::closeLoginDialog()
{
    if (m_loginPollTimer) {
        m_loginPollTimer->stop();
        m_loginPollTimer->deleteLater();
        m_loginPollTimer = nullptr;
    }
    if (m_loginDialog) {
        m_loginDialog->accept();
    }
}

void VideoPlayback_Weight::onLoginStatusChanged(int code, const QString &message, const QString &cookie)
{
    Q_UNUSED(message)
    if (!m_loginDialog)
        return;

    switch (code) {
    case 0:  //登录成功
        if (m_loginPollTimer)
            m_loginPollTimer->stop();
        //读取"自动登录"复选框状态
        if (m_autoLoginCheck)
            m_rememberLogin = m_autoLoginCheck->isChecked();
        //未勾选"自动登录"时，从QSettings删除Cookie(仅当前会话有效)
        if (!m_rememberLogin) {
            QSettings settings;
            settings.remove("bili/cookie");
        }
        m_loginDialog->accept();
        updateLoginUI();
        Logger::instance()->debug("VideoPlayback", "B站登录成功");
        break;
    case 86090:  //已扫码等待确认
        if (m_hintLabel)
            m_hintLabel->setText(QStringLiteral("已扫码，请在手机上确认"));
        break;
    case 86101:  //未扫码
        break;
    case 86038:  //二维码失效
        if (m_loginPollTimer)
            m_loginPollTimer->stop();
        if (m_hintLabel) {
            m_hintLabel->setText(QStringLiteral("二维码已失效，请关闭后重试"));
            m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
        }
        break;
    default:
        break;
    }
}

void VideoPlayback_Weight::onShowLoginContextMenu(const QPoint &pos)
{
    if (!m_apiWorker || !m_apiWorker->isLoggedIn())
        return;

    QMenu menu(this);
    QAction *logoutAction = menu.addAction(QStringLiteral("退出登录"));

    QAction *selected = menu.exec(ui->loginBtn->mapToGlobal(pos));
    if (selected == logoutAction) {
        m_apiWorker->clearCookie();
        updateLoginUI();
        Logger::instance()->debug("VideoPlayback", "已退出B站登录");
    }
}

void VideoPlayback_Weight::updateLoginUI()
{
    if (!m_apiWorker || !m_apiWorker->isLoggedIn()) {
        ui->loginBtn->setText(QStringLiteral("登录"));
        ui->loginBtn->setStyleSheet("QPushButton { text-align: left; padding: 4px 8px; border: none; color: #888; }");
        ui->loginStatusLabel->setText(QStringLiteral("登录后可获取高清流"));
    } else {
        ui->loginBtn->setText(QStringLiteral("已登录"));
        ui->loginBtn->setStyleSheet("QPushButton { text-align: left; padding: 4px 8px; border: none; color: #00a1d6; }");
        ui->loginStatusLabel->setText(QStringLiteral("右键可退出登录"));
    }
}

// ============================================================
// 工具方法
// ============================================================

QString VideoPlayback_Weight::formatTime(qint64 ms) const
{
    int totalSec = static_cast<int>(ms / 1000);
    int m = totalSec / 60;
    int s = totalSec % 60;
    return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

QString VideoPlayback_Weight::generateTempPath() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString previewDir = baseDir + "/preview_cache";
    QDir().mkpath(previewDir);

    QString fileName = QStringLiteral("preview_%1_%2.mp4")
        .arg(m_currentData.videoInfo.avid)
        .arg(QDateTime::currentDateTime().toSecsSinceEpoch());

    return QDir(previewDir).filePath(fileName);
}

QString VideoPlayback_Weight::generateOnlineTempPath() const
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString previewDir = baseDir + "/preview_cache";
    QDir().mkpath(previewDir);

    QString fileName = QStringLiteral("online_%1_%2.mp4")
        .arg(m_currentBvid)
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

void VideoPlayback_Weight::cleanupOnlineTempFile()
{
    if (!m_onlineTempPath.isEmpty()) {
        QFile file(m_onlineTempPath);
        if (file.exists()) {
            if (file.remove()) {
                Logger::instance()->debug("VideoPlayback",
                    QString("已删除在线临时文件: %1").arg(m_onlineTempPath));
            }
        }
        m_onlineTempPath.clear();
    }
}

void VideoPlayback_Weight::closeEvent(QCloseEvent *event)
{
    stopPlayers();
    cleanupTempFile();
    cleanupOnlineTempFile();
    m_ffmpeg->stopMux();

    Logger::instance()->debug("VideoPlayback", "预览窗口已关闭，临时文件已清理");
    QWidget::closeEvent(event);
}