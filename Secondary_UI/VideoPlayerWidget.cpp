#include "VideoPlayerWidget.h"
#include "ui_VideoPlayerWidget.h"
#include <QStyle>
#include <QVideoWidget>

VideoPlayerWidget::VideoPlayerWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayerWidget)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    ui->setupUi(this);

    //播放引擎关联到 .ui 中的 QVideoWidget
    m_player->setVideoOutput(ui->videoWidget);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.7f);

    //初始音量滑块
    ui->volumeSlider->setRange(0, 100);
    ui->volumeSlider->setValue(70);

    //初始播放按钮图标
    ui->playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->volumeLabel->setPixmap(style()->standardIcon(QStyle::SP_MediaVolume).pixmap(16, 16));

    //=== 信号槽 ===
    connect(ui->playBtn, &QPushButton::clicked, this, &VideoPlayerWidget::onPlayButtonClicked);
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayerWidget::onDurationChanged);
    connect(ui->progressSlider, &QSlider::sliderMoved, this, &VideoPlayerWidget::onSliderMoved);
    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_audioOutput->setVolume(v / 100.0f);
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, &VideoPlayerWidget::onMediaErrorOccurred);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoPlayerWidget::onMediaStatusChanged);
}

VideoPlayerWidget::~VideoPlayerWidget()
{
    delete ui;
}

void VideoPlayerWidget::loadFile(const QString &path)
{
    m_currentSource = path;
    m_player->setSource(QUrl::fromUserInput(path));
    ui->timeLabel->setText("00:00 / 00:00");
    ui->progressSlider->setValue(0);
}

void VideoPlayerWidget::stop()
{
    m_player->stop();
    m_player->setSource(QUrl());
    m_currentSource.clear();
    ui->timeLabel->setText("00:00 / 00:00");
    ui->progressSlider->setValue(0);
    ui->playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void VideoPlayerWidget::setLabel(const QString &text)
{
    ui->titleLabel->setText(text);
}

void VideoPlayerWidget::onPlayButtonClicked()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        ui->playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        m_player->play();
        ui->playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void VideoPlayerWidget::onPositionChanged(qint64 position)
{
    if (!m_isDragging) {
        ui->progressSlider->setValue(static_cast<int>(position));
    }
    qint64 duration = m_player->duration();
    ui->timeLabel->setText(QStringLiteral("%1 / %2").arg(formatTime(position), formatTime(duration)));
}

void VideoPlayerWidget::onDurationChanged(qint64 duration)
{
    ui->progressSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayerWidget::onSliderMoved(int position)
{
    m_player->setPosition(position);
}

void VideoPlayerWidget::onMediaErrorOccurred()
{
    QString error = m_player->errorString();
    if (error.isEmpty())
        error = QStringLiteral("未知播放错误");
    emit mediaLoaded(false, error);
}

void VideoPlayerWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::LoadedMedia) {
        emit mediaLoaded(true, QString());
    }
}

QString VideoPlayerWidget::formatTime(qint64 ms) const
{
    int totalSec = static_cast<int>(ms / 1000);
    int m = totalSec / 60;
    int s = totalSec % 60;
    return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}
