#include "TaskQueue.h"
#include "DataModel.h"
#include "logger.h"
#include <algorithm>

TaskQueue::TaskQueue(DataModel *dataModel, QObject *parent)
    : QObject(parent)
    , m_dataModel(dataModel)
    , m_ffmpeg(new FFmpeg_module(this))
    , m_currentIndex(0)
    , m_successCount(0)
    , m_failCount(0)
    , m_running(false)
{
    connect(m_ffmpeg, &FFmpeg_module::progressUpdated,
            this, &TaskQueue::onTaskProgress);
    connect(m_ffmpeg, &FFmpeg_module::finished,
            this, &TaskQueue::onTaskFinished);
}

void TaskQueue::start(const QList<MuxRequest> &requests, const QList<int> &rowIndices)
{
    if (m_running) {
        Logger::instance()->warning("TaskQueue", "队列已在运行，忽略重复启动请求");
        return;
    }

    m_requests = requests;
    m_rowIndices = rowIndices;
    m_completedRows.clear();
    m_currentIndex = 0;
    m_successCount = 0;
    m_failCount = 0;

    if (m_requests.isEmpty()) {
        Logger::instance()->debug("TaskQueue", "队列为空，无需执行");
        emit allFinished(0, 0);
        return;
    }

    m_running = true;
    Logger::instance()->debug("TaskQueue",
        QString("队列启动，共 %1 个任务").arg(m_requests.size()));
    processNext();
}

void TaskQueue::stop()
{
    if (!m_running)
        return;

    m_ffmpeg->stopMux();
    m_failCount += m_requests.size() - m_currentIndex - 1;  //剩余未执行的计为失败
    m_running = false;

    Logger::instance()->debug("TaskQueue",
        QString("队列已停止：成功 %1，失败 %2").arg(m_successCount).arg(m_failCount));
    emit allFinished(m_successCount, m_failCount);
}

void TaskQueue::processNext()
{
    if (m_currentIndex >= m_requests.size()) {
        m_running = false;

        //全部任务结束后，删除成功导出的行(降序删除避免索引错位)
        if (!m_completedRows.isEmpty()) {
            std::sort(m_completedRows.begin(), m_completedRows.end(), std::greater<int>());
            for (int row : m_completedRows) {
                m_dataModel->removeRow(row);
            }
            Logger::instance()->debug("TaskQueue",
                QString("已删除 %1 条已完成的行").arg(m_completedRows.size()));
        }

        Logger::instance()->debug("TaskQueue",
            QString("队列全部完成：成功 %1，失败 %2").arg(m_successCount).arg(m_failCount));
        emit allFinished(m_successCount, m_failCount);
        return;
    }

    emit taskStarted(m_currentIndex, m_requests.size());

    //将对应行状态设为导出中，进度0%
    if (m_currentIndex < m_rowIndices.size()) {
        m_dataModel->setExportProgress(m_rowIndices[m_currentIndex], 0);
    }

    Logger::instance()->debug("TaskQueue",
        QString("开始执行任务 %1/%2").arg(m_currentIndex + 1).arg(m_requests.size()));
    m_ffmpeg->startMux(m_requests[m_currentIndex]);
}

void TaskQueue::onTaskProgress(int percent)
{
    if (m_currentIndex < m_rowIndices.size()) {
        m_dataModel->setExportProgress(m_rowIndices[m_currentIndex], percent);
    }
    emit taskProgress(m_currentIndex, percent);
}

void TaskQueue::onTaskFinished(bool success, const QString &message)
{
    if (m_currentIndex < m_rowIndices.size()) {
        if (success) {
            m_dataModel->setExportProgress(m_rowIndices[m_currentIndex], 100);
            m_completedRows.append(m_rowIndices[m_currentIndex]);
        }
    }

    if (success)
        m_successCount++;
    else
        m_failCount++;

    Logger::instance()->debug("TaskQueue",
        QString("任务 %1 完成: %2 (%3)")
            .arg(m_currentIndex + 1)
            .arg(success ? "成功" : "失败")
            .arg(message));

    emit taskFinished(m_currentIndex, success, message);

    m_currentIndex++;
    processNext();
}
