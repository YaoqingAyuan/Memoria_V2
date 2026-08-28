#ifndef VIDEOPLAYBACK_WEIGHT_H
#define VIDEOPLAYBACK_WEIGHT_H
//预览播放窗口(VideoPlayback_Weight)
//三栏QSplitter布局：本地缓存播放 | 在线对比播放 | 搜索侧边栏
//底部状态栏：下架检测结果 + 对比信息
//临时文件管理：本地播放前FFmpeg流拷贝混流为临时MP4，关闭/删行时删除
//UI组件在 .ui 文件中设计(含提升的自定义控件)，此处仅处理业务逻辑

#include <QWidget>
#include <QProcess>
#include "Core/ParsedCacheData.h"
#include "Net_Module/BiliSearchResult.h"

class VideoPlayerWidget;
class WebSearchSidebar;
class BiliApiWorker;
class FFmpeg_module;

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
    //本地混流完成
    void onLocalMuxFinished(bool success, const QString &message);
    //搜索结果被选中 → 加载在线播放
    void onOnlineResultSelected(const BiliSearchResult &result);
    //下架检测结果更新
    void onAvailabilityUpdated(bool isAvailable, const QString &description);
    //在线播放地址获取成功
    void onPlayUrlReady(const QString &bvid, const QString &playUrl);

private:
    Ui::VideoPlayback_Weight *ui;

    //非UI成员
    BiliApiWorker *m_apiWorker;
    FFmpeg_module *m_ffmpeg;

    //当前数据
    ParsedCacheData m_currentData;
    QString m_tempFilePath;     //本地混流临时文件路径
    QString m_currentBvid;      //当前BV号

    //=== 临时文件管理 ===
    //生成临时文件路径
    QString generateTempPath() const;
    //删除临时文件
    void cleanupTempFile();
    //启动本地FFmpeg流拷贝混流
    void startLocalMux();
};

#endif // VIDEOPLAYBACK_WEIGHT_H
