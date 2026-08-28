#ifndef VIDEOPLAYERWIDGET_H
#define VIDEOPLAYERWIDGET_H
//视频(Video)播放器(Player)组件(Widget)：封装单个播放器
//包含画面区(QVideoWidget) + 底部控制条(播放/暂停/进度条/时间/音量)
//可加载本地文件路径或网络流地址
//UI组件在 .ui 文件中设计，此处仅处理播放逻辑

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>

class QVideoWidget;
class QSlider;
class QPushButton;
class QLabel;

namespace Ui {
class VideoPlayerWidget;
}

class VideoPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoPlayerWidget(QWidget *parent = nullptr);
    ~VideoPlayerWidget();

    //加载文件(本地路径或网络URL)
    void loadFile(const QString &path);
    //停止播放并清空
    void stop();

    //设置标签文字(如"本地缓存"/"在线对比")
    void setLabel(const QString &text);

    //获取当前媒体源路径
    QString currentSource() const { return m_currentSource; }

signals:
    //播放状态变化
    void playStateChanged(bool playing);
    //媒体加载完成(成功/失败)
    void mediaLoaded(bool success, const QString &error);

private slots:
    void onPlayButtonClicked();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onSliderMoved(int position);
    void onMediaErrorOccurred();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    Ui::VideoPlayerWidget *ui;

    //非UI成员：播放引擎
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;

    QString m_currentSource;
    bool m_isDragging = false;  //用户是否正在拖拽进度条

    //格式化时间 mm:ss
    QString formatTime(qint64 ms) const;
};

#endif // VIDEOPLAYERWIDGET_H
