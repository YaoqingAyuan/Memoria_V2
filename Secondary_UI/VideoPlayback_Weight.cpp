#include "VideoPlayback_Weight.h"
#include "ui_VideoPlayback_Weight.h"
#include "LoginManager.h"
#include "Net_Module/BiliApiWorker.h"
#include "Net_Module/HttpProxyServer.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "core/logger.h"
#include "core/utils.h"
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
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include "ResultCardWidget.h"

VideoPlayback_Weight::VideoPlayback_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayback_Weight)
    , m_nam(new QNetworkAccessManager(this))
    , m_localPlayer(new QMediaPlayer(this))
    , m_localAudio(new QAudioOutput(this))
    , m_onlinePlayer(new QMediaPlayer(this))
    , m_onlineAudio(new QAudioOutput(this))
    , m_apiWorker(new BiliApiWorker(m_nam, this))
    , m_ffmpeg(new FFmpeg_module(this))
    , m_proxy(new HttpProxyServer(m_nam, this))
    , m_loginManager(new LoginManager(m_apiWorker, this))
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

    //分P下拉框默认隐藏，有分P数据时显示(控件已在.ui中定义)
    ui->onlinePageCombo->setVisible(false);
    connect(ui->onlinePageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoPlayback_Weight::onOnlinePageChanged);

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
    connect(m_loginManager, &LoginManager::loginChanged, this, [this]() { updateLoginUI(); });

    //=== 信号槽：登录 ===
    connect(ui->loginBtn, &QPushButton::clicked, this, &VideoPlayback_Weight::onLoginBtnClicked);
    connect(ui->loginBtn, &QWidget::customContextMenuRequested, this, &VideoPlayback_Weight::onShowLoginContextMenu);

    updateLoginUI();

    Logger::instance()->debug("VideoPlayback", "VideoPlayback_Weight 初始化完成");
}

VideoPlayback_Weight::~VideoPlayback_Weight()
{
    cleanupTempFile();
    qDeleteAll(m_cardPool);
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
    QNetworkReply *reply = m_nam->get(request);
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
    m_currentCid = data.videoInfo.page_ep_Data.cid;
    m_currentPage = data.videoInfo.page_ep_Data.page;
    if (m_currentPage <= 0)
        m_currentPage = 1;

    //重置分P下拉框
    m_currentPages.clear();
    ui->onlinePageCombo->blockSignals(true);
    ui->onlinePageCombo->clear();
    ui->onlinePageCombo->setVisible(false);
    ui->onlinePageCombo->blockSignals(false);
    //打开预览时自动搜索，优先级：BV号 > AV号 > 标题
    //BV号非空 → searchById(isBvid=true)（同时触发下架检测）
    //BV号空但有AV号 → searchById(isBvid=false)（view API支持aid参数）
    //都没有 → 用标题模糊搜索（清理文件后缀后搜索）
    if (!m_currentBvid.isEmpty()) {
        ui->statusBar->setText(QStringLiteral("  正在搜索并检测下架状态..."));
        ui->searchEdit->setText(m_currentBvid);
        m_apiWorker->searchById(m_currentBvid, true);
    } else if (data.videoInfo.avid > 0) {
        ui->statusBar->setText(QStringLiteral("  正在按AV号搜索..."));
        ui->searchEdit->setText(QStringLiteral("av%1").arg(data.videoInfo.avid));
        m_apiWorker->searchById(QString::number(data.videoInfo.avid), false);
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
    //若尚未加载有效视频且有可用的CID，直接获取播放地址
    QMediaPlayer::MediaStatus status = m_onlinePlayer->mediaStatus();
    if ((status == QMediaPlayer::NoMedia || status == QMediaPlayer::InvalidMedia)) {
        //防止重复请求
        if (m_isFetchingPlayUrl || m_isOnlineMuxing) {
            ui->statusBar->setText(QStringLiteral("  正在缓冲中，请稍候..."));
            return;
        }
        //优先使用当前选中的分P CID
        if (m_currentCid > 0 && !m_currentBvid.isEmpty()) {
            m_isFetchingPlayUrl = true;
            if (m_apiWorker->isLoggedIn()) {
                m_apiWorker->fetchPlayUrlDash(m_currentBvid, static_cast<int>(m_currentCid));
            } else {
                m_apiWorker->fetchPlayUrl(m_currentBvid, static_cast<int>(m_currentCid), 32);
            }
            ui->statusBar->setText(QStringLiteral("  正在获取在线播放地址..."));
            return;
        }
        //回退：有搜索结果时自动播放首位视频
        if (!m_results.isEmpty()) {
            onOnlineResultSelected(m_results.first());
            return;
        }
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
        m_apiWorker->searchById(keyword, true);
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
        //清空分P下拉框
        ui->onlinePageCombo->blockSignals(true);
        ui->onlinePageCombo->clear();
        ui->onlinePageCombo->setVisible(false);
        ui->onlinePageCombo->blockSignals(false);
        m_currentPages.clear();
        return;
    }

    //复用m_cardPool中的卡片，不足时新建
    for (int i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        ResultCardWidget *card;
        if (i < m_cardPool.size()) {
            //复用已有卡片
            card = m_cardPool[i];
            card->updateData(r);
        } else {
            //池中不够，新建并加入池
            card = new ResultCardWidget(r, ui->resultContainer);
            m_cardPool.append(card);
            connect(card, &ResultCardWidget::clicked, this, [this, card]() {
                onOnlineResultSelected(card->data());
            });
        }
        card->show();
        ui->resultCardsLayout->addWidget(card);
        m_resultCards.append(card);
        //封面：bvid不变时复用已有封面，仅新bvid才下载
        if (!r.coverUrl.isEmpty() && !card->hasCover())
            fetchCover(r.bvid, r.coverUrl);
    }

    //末尾弹性空间, 让卡片左对齐
    ui->resultCardsLayout->addStretch();
    //更新容器最小宽度, 超出视口时自动出现水平滚动条
    ui->resultContainer->setMinimumWidth(results.size() * 124 + 8);

    ui->statusBar->setText(QStringLiteral("  找到 %1 条结果").arg(results.size()));

    //如果第一条结果有分P列表(view接口返回)，填充分P下拉框
    const BiliSearchResult &first = results.first();
    if (!first.pages.isEmpty()) {
        m_currentPages = first.pages;
        ui->onlinePageCombo->blockSignals(true);
        ui->onlinePageCombo->clear();
        for (const BiliPageInfo &pi : first.pages) {
            QString label = QStringLiteral("P%1 · %2").arg(pi.page).arg(pi.part);
            ui->onlinePageCombo->addItem(label, pi.cid);
        }
        //自动匹配当前缓存对应的分P
        int matchIndex = -1;
        if (m_currentCid > 0) {
            //优先按CID匹配
            for (int i = 0; i < first.pages.size(); ++i) {
                if (first.pages.at(i).cid == m_currentCid) {
                    matchIndex = i;
                    break;
                }
            }
        }
        if (matchIndex < 0 && m_currentPage > 0) {
            //CID未匹配则按P号匹配
            for (int i = 0; i < first.pages.size(); ++i) {
                if (first.pages.at(i).page == m_currentPage) {
                    matchIndex = i;
                    break;
                }
            }
        }
        if (matchIndex >= 0) {
            ui->onlinePageCombo->setCurrentIndex(matchIndex);
            m_currentCid = first.pages.at(matchIndex).cid;
            m_currentPage = first.pages.at(matchIndex).page;
        } else {
            ui->onlinePageCombo->setCurrentIndex(0);
            m_currentCid = first.pages.first().cid;
            m_currentPage = first.pages.first().page;
        }
        ui->onlinePageCombo->setVisible(true);
        ui->onlinePageCombo->blockSignals(false);

        Logger::instance()->debug("VideoPlayback",
            QString("分P列表已加载: 共%1P, 当前选中P%2 (CID=%3)")
                .arg(first.pages.size()).arg(m_currentPage).arg(m_currentCid));
    }
}

void VideoPlayback_Weight::onSearchFailed(const QString &error)
{
    ui->statusBar->setText(QStringLiteral("  ❌ 搜索失败: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("搜索失败: %1").arg(error));
}

void VideoPlayback_Weight::clearResultCards()
{
    //从布局移除所有项(不删除widget，移入m_cardPool供下次复用)
    while (ui->resultCardsLayout->count() > 0) {
        QLayoutItem *item = ui->resultCardsLayout->takeAt(0);
        delete item;  //仅删除LayoutItem，widget保留在m_cardPool中
    }
    for (ResultCardWidget *card : m_cardPool)
        card->hide();
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

    ui->statusBar->setText(QStringLiteral("  正在获取在线播放地址..."));
    m_currentBvid = result.bvid;

    //如果搜索结果有分P列表，更新分P下拉框
    if (!result.pages.isEmpty()) {
        m_currentPages = result.pages;
        ui->onlinePageCombo->blockSignals(true);
        ui->onlinePageCombo->clear();
        for (const BiliPageInfo &pi : result.pages) {
            QString label = QStringLiteral("P%1 · %2").arg(pi.page).arg(pi.part);
            ui->onlinePageCombo->addItem(label, pi.cid);
        }
        //选中第一P
        ui->onlinePageCombo->setCurrentIndex(0);
        m_currentCid = result.pages.first().cid;
        m_currentPage = result.pages.first().page;
        ui->onlinePageCombo->setVisible(true);
        ui->onlinePageCombo->blockSignals(false);

        Logger::instance()->debug("VideoPlayback",
            QString("选中搜索结果，分P列表已更新: 共%1P").arg(result.pages.size()));
    } else {
        //无分P列表(关键词搜索结果)，使用结果的cid
        m_currentCid = result.cid;
        m_currentPage = 1;
        m_currentPages.clear();
        ui->onlinePageCombo->blockSignals(true);
        ui->onlinePageCombo->clear();
        ui->onlinePageCombo->setVisible(false);
        ui->onlinePageCombo->blockSignals(false);
    }

    if (m_currentCid <= 0) {
        ui->statusBar->setText(QStringLiteral("  ❌ 搜索结果无CID"));
        return;
    }

    m_isFetchingPlayUrl = true;

    if (m_apiWorker->isLoggedIn()) {
        //已登录：DASH格式获取最高画质(1080P/4K)，FFmpeg混流后播放
        m_apiWorker->fetchPlayUrlDash(m_currentBvid, static_cast<int>(m_currentCid));
    } else {
        //未登录：MP4格式(480P)，通过代理即时播放
        m_apiWorker->fetchPlayUrl(m_currentBvid, static_cast<int>(m_currentCid), 32);
    }
}

void VideoPlayback_Weight::onPlayUrlReady(const QString &bvid, const QString &playUrl)
{
    //方案B: 通过本地HTTP代理注入Referer/Cookie头，绕过B站CDN防盗链
    //方案C(URL查询参数注入)因QUrl百分号编码导致Referer值失效，已弃用
    Q_UNUSED(bvid);
    m_isFetchingPlayUrl = false;
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
    Q_UNUSED(bvid);
    m_isFetchingPlayUrl = false;
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
    m_isFetchingPlayUrl = false;
    ui->statusBar->setText(QStringLiteral("  在线播放失败: %1").arg(error));
    Logger::instance()->warning("VideoPlayback", QString("在线播放地址获取失败: %1").arg(error));
}

// ============================================================
// 分P切换
// ============================================================

void VideoPlayback_Weight::onOnlinePageChanged(int index)
{
    if (index < 0 || index >= m_currentPages.size())
        return;

    const BiliPageInfo &page = m_currentPages.at(index);
    m_currentCid = page.cid;
    m_currentPage = page.page;

    Logger::instance()->debug("VideoPlayback",
        QString("切换到分P: P%1, CID=%2, 标题=%3")
            .arg(page.page).arg(page.cid).arg(page.part));

    //停止当前在线播放，重新获取新P的播放地址
    m_onlinePlayer->stop();
    m_onlinePlayer->setSource(QUrl());
    //停止可能正在运行的在线混流
    if (m_isOnlineMuxing) {
        m_ffmpeg->stopMux();
        m_isOnlineMuxing = false;
    }
    cleanupOnlineTempFile();
    ui->onlineTimeLabel->setText("00:00 / 00:00");
    ui->onlineProgressSlider->setValue(0);
    ui->onlinePlayBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

    if (m_currentBvid.isEmpty() || m_currentCid <= 0) {
        ui->statusBar->setText(QStringLiteral("  ❌ 无法获取该P的播放地址"));
        return;
    }

    ui->statusBar->setText(QStringLiteral("  正在获取P%1播放地址...").arg(page.page));
    m_isFetchingPlayUrl = true;

    if (m_apiWorker->isLoggedIn()) {
        m_apiWorker->fetchPlayUrlDash(m_currentBvid, static_cast<int>(m_currentCid));
    } else {
        m_apiWorker->fetchPlayUrl(m_currentBvid, static_cast<int>(m_currentCid), 32);
    }
}

// ============================================================
// B站登录
// ============================================================

void VideoPlayback_Weight::onLoginBtnClicked()
{
    m_loginManager->startLogin();
}

void VideoPlayback_Weight::onShowLoginContextMenu(const QPoint &pos)
{
    m_loginManager->showContextMenu(pos, ui->loginBtn);
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