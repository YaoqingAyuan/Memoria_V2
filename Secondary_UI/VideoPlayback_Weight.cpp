#include "VideoPlayback_Weight.h"
#include "ui_VideoPlayback_Weight.h"
#include "Net_Module/BiliApiWorker.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include <QStyle>
#include <QVideoWidget>
#include <QSlider>
#include <QListWidgetItem>
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

VideoPlayback_Weight::VideoPlayback_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayback_Weight)
    , m_localPlayer(new QMediaPlayer(this))
    , m_localAudio(new QAudioOutput(this))
    , m_onlinePlayer(new QMediaPlayer(this))
    , m_onlineAudio(new QAudioOutput(this))
    , m_apiWorker(new BiliApiWorker(this))
    , m_ffmpeg(new FFmpeg_module(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("预览 — 双屏播放对比"));
    resize(1200, 700);

    //布局stretch因子(不能在.ui中设置，UIC会生成错误的setStretch(QString)调用)
    //主布局: 播放器=4, 搜索栏=0(固定高), 结果列表=5, 状态栏=0(固定高)
    ui->mainLayout->setStretch(0, 4);
    ui->mainLayout->setStretch(1, 0);
    ui->mainLayout->setStretch(2, 5);
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
    connect(ui->resultList, &QListWidget::itemClicked, this, &VideoPlayback_Weight::onResultItemClicked);

    //=== 信号槽：BiliApiWorker ===
    connect(m_apiWorker, &BiliApiWorker::searchResultReady, this, &VideoPlayback_Weight::onSearchResultReady);
    connect(m_apiWorker, &BiliApiWorker::searchFailed, this, &VideoPlayback_Weight::onSearchFailed);
    connect(m_apiWorker, &BiliApiWorker::availabilityChecked, this, &VideoPlayback_Weight::onAvailabilityChecked);
    connect(m_apiWorker, &BiliApiWorker::playUrlReady, this, &VideoPlayback_Weight::onPlayUrlReady);
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

// ============================================================
// 加载缓存数据
// ============================================================

void VideoPlayback_Weight::loadCacheData(const ParsedCacheData &data)
{
    m_currentData = data;

    setWindowTitle(QStringLiteral("预览 — %1").arg(data.videoInfo.title));

    startLocalMux();

    m_currentBvid = data.videoInfo.bvid;
    if (!m_currentBvid.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  正在检测下架状态..."));
        m_apiWorker->checkAvailability(m_currentBvid);
        ui->searchEdit->setText(m_currentBvid);
    } else {
        ui->statusBar->setText(QStringLiteral("  无BV号，跳过下架检测"));
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

void VideoPlayback_Weight::loadOnlineFile(const QString &path)
{
    m_onlinePlayer->setSource(QUrl::fromUserInput(path));
    ui->onlineTimeLabel->setText("00:00 / 00:00");
    ui->onlineProgressSlider->setValue(0);
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

// ============================================================
// 搜索
// ============================================================

void VideoPlayback_Weight::onSearchClicked()
{
    QString keyword = ui->searchEdit->text().trimmed();
    if (keyword.isEmpty())
        return;

    ui->availabilityLabel->setText(QStringLiteral("搜索中..."));
    ui->resultList->clear();
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
    ui->resultList->clear();

    if (results.isEmpty()) {
        ui->availabilityLabel->setText(QStringLiteral("无搜索结果"));
        return;
    }

    for (const auto &r : results) {
        QString display = QStringLiteral("%1\n%2 · %3 播放")
            .arg(r.title)
            .arg(r.ownerName)
            .arg(r.formattedPlayCount());
        QListWidgetItem *item = new QListWidgetItem(display, ui->resultList);
        if (!r.isAvailable)
            item->setForeground(Qt::red);
    }

    ui->availabilityLabel->setText(QStringLiteral("找到 %1 条结果").arg(results.size()));
}

void VideoPlayback_Weight::onSearchFailed(const QString &error)
{
    ui->availabilityLabel->setText(QStringLiteral("搜索失败: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("搜索失败: %1").arg(error));
}

void VideoPlayback_Weight::onResultItemClicked(QListWidgetItem *item)
{
    int row = ui->resultList->row(item);
    if (row >= 0 && row < m_results.size()) {
        onOnlineResultSelected(m_results[row]);
    }
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
    ui->statusBar->setText(QStringLiteral("  正在获取在线播放地址..."));

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
    loadOnlineFile(playUrl);
    Logger::instance()->debug("VideoPlayback",
        QString("在线播放地址已加载: %1").arg(playUrl.left(80)));
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

    QCheckBox *autoLoginCheck = new QCheckBox(QStringLiteral("自动登录(以后打开软件时保持登录)"), m_loginDialog);
    autoLoginCheck->setChecked(true);
    layout->addWidget(autoLoginCheck);

    //下载二维码图片
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    QNetworkRequest request((QUrl(qrImageUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"));
    QNetworkReply *reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (m_hintLabel)
                m_hintLabel->setText(QStringLiteral("二维码加载失败"));
            return;
        }
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull() && m_qrCodeLabel) {
            m_qrCodeLabel->setPixmap(pixmap.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });

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
    stopPlayers();
    cleanupTempFile();
    m_ffmpeg->stopMux();

    Logger::instance()->debug("VideoPlayback", "预览窗口已关闭，临时文件已清理");
    QWidget::closeEvent(event);
}
