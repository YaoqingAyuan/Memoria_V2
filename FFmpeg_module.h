#ifndef FFMPEG_MODULE_H
#define FFMPEG_MODULE_H
//FFmpeg模块类
/* 职责：接收音视频文件地址 + UI设置的输出地址，完成.m4s音视频混流
 * 多数情况"复制流"不转码(MP4/MKV/MOV)；少数需转码(WEBM,需UI传参)
 *
 * 设计依据：FFmpeg模块架构.pdf
 *   1.自检验证 → 2.解析传入地址 → 3.构建指令 → 4.异步执行 → 5.输出
 *
 * AVI格式已移除(过于老旧且边缘)；MOV格式取代(容器支持H.264/AAC,复制流即可)
 */

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include "ParsedCacheData.h"

//输出格式枚举(对应UI"格式选择对话框"的4种选项)
enum class OutputFormat {
    MP4,    //.mp4  — 复制流，无需转码
    MKV,    //.mkv  — 复制流，无需转码
    MOV,    //.mov  — 复制流，无需转码(新增，取代已移除的AVI)
    WEBM    //.webm — 需转码(VP9+Opus)，需用户在UI填写参数传入
};

//转码参数结构体：仅在WEBM等需转码格式时由UI填充并传入
struct TranscodeParams {
    //WEBM(VP9)参数
    int crf = 30;           //恒定质量(0~63，30为平衡点)
    int cpuUsed = 2;        //编码速度/质量权衡(0=慢质高，5=快速低质)
    int audioBitrate = 128; //音频码率(kbps)
    int threads = 4;        //编码线程数(按CPU核心数调整)

    //将转码参数序列化为FFmpeg命令行参数列表
    QStringList toArgs() const;
};

//混流任务请求结构体：由调用方(UI/任务队列)组装后传入
struct MuxRequest {
    QString videoPath;      //video.m4s路径(来自ParsedCacheData.videoInfo.videoFilePath)
    QString audioPath;      //audio.m4s路径(来自ParsedCacheData.videoInfo.audioFilePath)
    QString outputPath;     //输出文件路径(UI设置)
    OutputFormat format;    //输出格式
    TranscodeParams params; //转码参数(仅WEBM等转码格式有效)
};

class FFmpeg_module : public QObject
{
    Q_OBJECT
public:
    explicit FFmpeg_module(QObject *parent = nullptr);
    ~FFmpeg_module();

    // ========== 自检验证(初始化事件) ==========
    //检测FFmpeg环境：优先使用用户设备已存在的环境，否则调用软件自带环境
    //返回ffmpeg.exe的完整路径
    QString selfCheck();

    // ========== 解析传入地址 + 构建指令 ==========
    //根据MuxRequest构建完整的FFmpeg命令行参数列表
    //内部按format分支：MP4/MKV/MOV走复制流，WEBM走转码
    QStringList buildCommand(const MuxRequest &request);

    // ========== 混流进程启动(用户UI操作) ==========
    //核心入口：接收请求，构建指令，打印日志，异步启动QProcess
    void startMux(const MuxRequest &request);

    //停止当前混流进程(尝试优雅退出，失败则强制终止)
    void stopMux();

signals:
    //进度更新信号(0~100百分比)
    void progressUpdated(int percentage);

    //任务完成信号(success=是否成功, message=完成提示或错误信息)
    void finished(bool success, const QString &message);

    //实时日志输出信号(FFmpeg原始stderr输出，供UI日志面板显示)
    void logOutput(const QString &log);

private slots:
    //处理FFmpeg标准错误输出(进度信息通常在stderr中)
    void onReadyReadStandardError();

    //处理进程结束
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;                //FFmpeg进程
    QString m_ffmpegPath;               //ffmpeg.exe完整路径(selfCheck后确定)
    double m_totalDuration;             //视频总时长(秒)，用于计算进度百分比
    QRegularExpression m_durationRegex; //匹配 Duration: HH:MM:SS.xx
    QRegularExpression m_timeRegex;     //匹配 time=HH:MM:SS.xx

    //格式指令构建私有函数(对应架构图"根据格式生成指令")
    QStringList buildCopyCommand(const MuxRequest &request, const QString &formatExt);
    QStringList buildWebmCommand(const MuxRequest &request);

    //从stderr文本中解析总时长(秒)
    double parseDuration(const QString &output);

    //从stderr文本中解析当前处理时间(秒)
    double parseCurrentTime(const QString &output);
};

#endif // FFMPEG_MODULE_H
