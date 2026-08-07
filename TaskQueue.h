#ifndef TASKQUEUE_H
#define TASKQUEUE_H
//任务(Task)队列(Queue)类
/* 职责：接收一组MuxRequest，顺序驱动FFmpeg_module逐个执行
 * 每个任务异步执行(QProcess)，完成后自动启动下一个
 * 执行过程中实时更新DataModel对应行的导出进度
 */

#include <QObject>
#include <QList>
#include "FFmpeg_module.h"    //MuxRequest定义

class DataModel;

class TaskQueue : public QObject
{
    Q_OBJECT
public:
    explicit TaskQueue(DataModel *dataModel, QObject *parent = nullptr);

    //启动时自检FFmpeg环境(委托FFmpeg_module::selfCheck)，返回ffmpeg.exe完整路径(空串=未找到)
    //在MainWindow构造时调用一次：既预填m_ffmpegPath供后续startMux复用，又供UI更新状态标签
    QString selfCheckFFmpeg();

    //启动队列：requests与rowIndices一一对应(行索引用于更新进度)
    void start(const QList<MuxRequest> &requests, const QList<int> &rowIndices);

    //停止队列(终止当前任务，放弃后续任务)
    void stop();

    bool isRunning() const { return m_running; }

signals:
    //单个任务开始(从0开始计数)
    void taskStarted(int taskIndex, int totalTasks);
    //单个任务进度更新
    void taskProgress(int taskIndex, int percent);
    //单个任务完成
    void taskFinished(int taskIndex, bool success, const QString &message);
    //全部任务完成(成功数, 失败数)
    void allFinished(int successCount, int failCount);

private slots:
    void onTaskProgress(int percent);
    void onTaskFinished(bool success, const QString &message);

private:
    DataModel *m_dataModel;
    FFmpeg_module *m_ffmpeg;

    QList<MuxRequest> m_requests;   //待执行的任务列表
    QList<int> m_rowIndices;        //每任务对应的表格行索引
    QList<int> m_completedRows;     //成功完成的行索引(全部结束后统一删除)
    int m_currentIndex;             //当前执行到第几个任务
    int m_successCount;             //成功计数
    int m_failCount;                //失败计数
    bool m_running;                 //是否正在执行

    //执行下一个任务(若已全部完成则发射allFinished)
    void processNext();
};

#endif // TASKQUEUE_H
