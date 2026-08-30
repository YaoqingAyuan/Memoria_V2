#ifndef VIDEOPLAYBACK_WEIGHT_H
#define VIDEOPLAYBACK_WEIGHT_H
//预览播放窗口(VideoPlayback_Weight)
//统一界面：双屏播放(本地|云端) → 搜索栏+登录+下架标识 → 搜索结果列表 → 状态栏
//所有UI组件在 .ui 文件中设计(含QVideoWidget提升控件)，此处处理全部业务逻辑
//临时文件管理：本地播放前FFmpeg流拷贝混流为临时MP4，关闭/删行时删除

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QProcess>
#include "Core/ParsedCacheData.h"
#include "Net_Module/BiliSearchResult.h"

class BiliApiWorker;
class FFmpeg_module;
class QListWidgetItem;
class QDialog;
class QLabel;
class QTimer;

namespace Ui {
class VideoPlayback_Weight;
}

class VideoPlayback_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayback_Weight(QWidget *parent = nullptr);
    ~VideoPlayback_Weight();

    //加载缓存数据：启动本地FFmpeg混流 + 在线下架检测
    void loadCacheData(const ParsedCacheData &data);

    //当前预览的 avid(供 MainWindow 删行时判断是否需清理)
    qint64 currentAvid() const { return m_currentData.videoInfo.avid; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    //=== 本地播放器 ===
    void onLocalPlayClicked();
    void onLocalRewindClicked();
    void onLocalForwardClicked();
    void onLocalPositionChanged(qint64 position);
    void onLocalDurationChanged(qint64 duration);
    void onLocalSliderMoved(int position);
    void onLocalMediaError();
    void onLocalMediaStatusChanged(QMediaPlayer::MediaStatus status);

    //=== 云端播放器 ===
    void onOnlinePlayClicked();
    void onOnlineRewindClicked();
    void onOnlineForwardClicked();
    void onOnlinePositionChanged(qint64 position);
    void onOnlineDurationChanged(qint64 duration);
    void onOnlineSliderMoved(int position);
    void onOnlineMediaError();
    void onOnlineMediaStatusChanged(QMediaPlayer::MediaStatus status);

    //=== FFmpeg混流 ===
    void onLocalMuxFinished(bool success, const QString &message);

    //=== 搜索 ===
    void onSearchClicked();
    void onSearchResultReady(const QList<BiliSearchResult> &results);
    void onSearchFailed(const QString &error);
    void onResultItemClicked(QListWidgetItem *item);

    //=== 下架检测 ===
    void onAvailabilityChecked(const QString &bvid, bool isAvailable, const QString &description);

    //=== 在线播放 ===
    void onOnlineResultSelected(const BiliSearchResult &result);
    void onPlayUrlReady(const QString &bvid, const QString &playUrl);

    //=== B站登录 ===
    void onLoginBtnClicked();
    void onLoginStatusChanged(int code, const QString &message, const QString &cookie);
    void onShowLoginContextMenu(const QPoint &pos);

private:
    Ui::VideoPlayback_Weight *ui;

    //播放引擎
    QMediaPlayer *m_localPlayer;
    QAudioOutput *m_localAudio;
    QMediaPlayer *m_onlinePlayer;
    QAudioOutput *m_onlineAudio;

    //API与FFmpeg
    BiliApiWorker *m_apiWorker;
    FFmpeg_module *m_ffmpeg;

    //当前数据
    ParsedCacheData m_currentData;
    QString m_tempFilePath;
    QString m_currentBvid;

    //搜索结果缓存(索引对应列表项)
    QList<BiliSearchResult> m_results;

    //播放器拖拽状态
    bool m_localDragging = false;
    bool m_onlineDragging = false;

    //登录对话框
    QDialog *m_loginDialog = nullptr;
    QLabel *m_qrCodeLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QString m_qrcodeKey;
    QTimer *m_loginPollTimer = nullptr;

    //=== 工具方法 ===
    QString formatTime(qint64 ms) const;
    void initPlayerIcons();
    void loadLocalFile(const QString &path);
    void loadOnlineFile(const QString &path);
    void stopPlayers();
    void updateLoginUI();
    void showLoginDialog(const QString &qrImageUrl);
    void closeLoginDialog();
    QString generateTempPath() const;
    void cleanupTempFile();
    void startLocalMux();
};

#endif // VIDEOPLAYBACK_WEIGHT_H
