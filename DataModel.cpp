#include "DataModel.h"
#include "logger.h"
#include <QDateTime>
#include <QSettings>

//默认的可选列可见性：离线ID=true, Bv号=false, 视频类型=false, Up主=true,
//Up主UID=false, 创建时间=false, 时长=false, 文件大小=false, 弹幕数=false,
//分辨率=true
const bool DataModel::s_defaultVisible[OptionalCount] = {
    true,   //ColAvid
    false,  //ColBvid
    false,  //ColVideoType
    true,   //ColOwnerName
    false,  //ColOwnerId
    false,  //ColCreateTime
    false,  //ColDuration
    false,  //ColFileSize
    false,  //ColDanmakuCount
    true    //ColResolution
};

DataModel::DataModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    //初始化可选列可见性为默认值
    for (int i = 0; i < OptionalCount; ++i) {
        m_optionalVisible[i] = s_defaultVisible[i];
    }
    rebuildColumnMap();
}

//=== QAbstractTableModel 虚函数实现 ===

int DataModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_rows.size();
}

int DataModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_visibleColumnMap.size();
}

QVariant DataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int visibleCol = index.column();

    if (row < 0 || row >= m_rows.size())
        return QVariant();

    if (visibleCol < 0 || visibleCol >= m_visibleColumnMap.size())
        return QVariant();

    int logicalCol = m_visibleColumnMap[visibleCol];

    //显示角色：返回单元格文本
    if (role == Qt::DisplayRole) {
        //强制列(0..7)
        if (logicalCol < MandatoryCount) {
            switch (logicalCol) {
            case ColSeq:
                return row + 1;     //序号=行索引+1
            case ColStatus: {
                //状态列：根据rowStatus返回中文标签
                switch (m_rows[row].rowStatus) {
                case Empty:      return QStringLiteral("待填写");
                case Valid:      return QStringLiteral("有效");
                case Invalid:    return QStringLiteral("无效");
                case Exporting:  return QStringLiteral("导出中");
                case Completed:  return QStringLiteral("已完成");
                }
                return QVariant();
            }
            case ColTitle:
                return sharedColumnText(row, logicalCol);
            case ColPageNo: {
                int page = m_rows[row].videoInfo.page_ep_Data.page;
                if (page == 0 && m_rows[row].rowStatus == Empty)
                    return QVariant();
                return page;
            }
            case ColPartTitle: {
                const QString &pt = m_rows[row].videoInfo.page_ep_Data.partTitle;
                if (pt.isEmpty() && m_rows[row].rowStatus == Empty)
                    return QVariant();
                return pt;
            }
            case ColVideoPath:
                return m_rows[row].videoInfo.videoFilePath;
            case ColAudioPath:
                return m_rows[row].videoInfo.audioFilePath;
            case ColProgress: {
                if (row < m_progress.size() && m_progress[row] >= 0)
                    return QStringLiteral("%1%").arg(m_progress[row]);
                return QVariant();
            }
            }
        } else {
            //可选列(8..18)：logicalCol-8 得到 OptionalColumn 枚举值
            int optCol = logicalCol - MandatoryCount;
            const ParsedCacheData &d = m_rows[row];

            //空行不显示可选列内容
            if (d.rowStatus == Empty)
                return QVariant();

            switch (optCol) {
            case ColAvid:
                if (d.videoInfo.avid == 0) return QVariant();
                return QString::number(d.videoInfo.avid);
            case ColBvid:
                return d.videoInfo.bvid;
            case ColVideoType:
                switch (d.videoInfo.videoType) {
                case Normal:  return QStringLiteral("普通视频");
                case Bangumi: return QStringLiteral("番剧");
                case Unknown: return QVariant();
                }
                return QVariant();
            case ColOwnerName:
                return sharedColumnText(row, logicalCol);
            case ColOwnerId:
                if (d.videoInfo.ownerId.isEmpty()) return QVariant();
                return d.videoInfo.ownerId;
            case ColCreateTime: {
                if (d.videoInfo.create_timestamp == 0) return QVariant();
                //时间戳转可读格式(yyyy-MM-dd hh:mm)
                QDateTime dt = QDateTime::fromSecsSinceEpoch(d.videoInfo.create_timestamp / 1000);
                return dt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
            }
            case ColDuration: {
                if (d.videoInfo.totalTimeMilli == 0) return QVariant();
                //毫秒转秒
                return QStringLiteral("%1s").arg(d.videoInfo.totalTimeMilli / 1000);
            }
            case ColFileSize: {
                if (d.videoInfo.totalBytes == 0) return QVariant();
                //比特转MB
                return QStringLiteral("%1 MB").arg(d.videoInfo.totalBytes / (1024.0 * 1024.0), 0, 'f', 2);
            }
            case ColDanmakuCount:
                if (d.videoInfo.recent_danmaku_count == 0) return QVariant();
                return d.videoInfo.recent_danmaku_count;
            case ColResolution: {
                int w = d.videoStream.width;
                int h = d.videoStream.height;
                if (w == 0 || h == 0) return QVariant();
                return QStringLiteral("%1×%2").arg(w).arg(h);
            }
            }
        }
    }

    //文本对齐角色：序号、P号、状态居中对齐
    if (role == Qt::TextAlignmentRole) {
        int logicalCol = m_visibleColumnMap[visibleCol];
        if (logicalCol == ColSeq || logicalCol == ColPageNo ||
            logicalCol == ColStatus || logicalCol == ColProgress) {
            return static_cast<int>(Qt::AlignCenter);
        }
        //数值类可选列居中
        int optCol = logicalCol - MandatoryCount;
        if (logicalCol >= MandatoryCount) {
            if (optCol == ColDanmakuCount || optCol == ColDuration ||
                optCol == ColFileSize || optCol == ColResolution)
                return static_cast<int>(Qt::AlignCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant DataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        if (section < 0 || section >= m_visibleColumnMap.size())
            return QVariant();

        int logicalCol = m_visibleColumnMap[section];

        //强制列表头
        if (logicalCol < MandatoryCount) {
            switch (logicalCol) {
            case ColSeq:        return QStringLiteral("序号");
            case ColStatus:     return QStringLiteral("状态");
            case ColTitle:      return QStringLiteral("标题");
            case ColPageNo:     return QStringLiteral("P号");
            case ColPartTitle:  return QStringLiteral("分P标题");
            case ColVideoPath:  return QStringLiteral("视频路径");
            case ColAudioPath:  return QStringLiteral("音频路径");
            case ColProgress:   return QStringLiteral("进度");
            }
        } else {
            //可选列表头
            int optCol = logicalCol - MandatoryCount;
            switch (optCol) {
            case ColAvid:           return QStringLiteral("离线ID");
            case ColBvid:           return QStringLiteral("Bv号");
            case ColVideoType:      return QStringLiteral("视频类型");
            case ColOwnerName:      return QStringLiteral("Up主");
            case ColOwnerId:        return QStringLiteral("Up主UID");
            case ColCreateTime:     return QStringLiteral("创建时间");
            case ColDuration:       return QStringLiteral("时长");
            case ColFileSize:       return QStringLiteral("文件大小");
            case ColDanmakuCount:   return QStringLiteral("弹幕数");
            case ColResolution:     return QStringLiteral("分辨率");
            }
        }
    }

    return QVariant();
}

//=== 行操作 ===

int DataModel::addEmptyRow()
{
    int newRow = m_rows.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_rows.append(ParsedCacheData());
    m_progress.append(-1);
    endInsertRows();
    return newRow;
}

void DataModel::insertEmptyRow(int row)
{
    if (row < 0 || row > m_rows.size())
        return;

    beginInsertRows(QModelIndex(), row, row);
    m_rows.insert(row, ParsedCacheData());
    m_progress.insert(row, -1);
    endInsertRows();
}

void DataModel::removeRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_rows.removeAt(row);
    m_progress.removeAt(row);
    endRemoveRows();
}

void DataModel::setRowData(int row, const ParsedCacheData &data)
{
    if (row < 0) {
        //追加到末尾
        int newRow = m_rows.size();
        beginInsertRows(QModelIndex(), newRow, newRow);
        m_rows.append(data);
        m_progress.append(-1);
        endInsertRows();
        updateRowStatus(newRow);
    } else if (row < m_rows.size()) {
        m_rows[row] = data;
        if (row >= m_progress.size())
            m_progress.resize(m_rows.size(), -1);
        m_progress[row] = -1;
        updateRowStatus(row);
    }
}

const ParsedCacheData& DataModel::getRowData(int row) const
{
    Q_ASSERT(row >= 0 && row < m_rows.size());
    return m_rows[row];
}

ParsedCacheData& DataModel::getRowDataRef(int row)
{
    Q_ASSERT(row >= 0 && row < m_rows.size());
    return m_rows[row];
}

void DataModel::refreshRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    updateRowStatus(row);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void DataModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_progress.clear();
    endResetModel();
}

//=== 列可见性 ===

bool DataModel::isOptionalColumnVisible(OptionalColumn col) const
{
    int idx = static_cast<int>(col);
    if (idx < 0 || idx >= OptionalCount)
        return false;
    return m_optionalVisible[idx];
}

void DataModel::setOptionalColumnVisible(OptionalColumn col, bool visible)
{
    int idx = static_cast<int>(col);
    if (idx < 0 || idx >= OptionalCount)
        return;

    if (m_optionalVisible[idx] == visible)
        return;

    //保存到配置
    QSettings settings;
    settings.setValue(QStringLiteral("Table/OptionalCol_%1").arg(idx), visible);

    //重建映射并通知视图
    beginResetModel();
    m_optionalVisible[idx] = visible;
    rebuildColumnMap();
    endResetModel();
}

void DataModel::loadColumnVisibility()
{
    QSettings settings;
    for (int i = 0; i < OptionalCount; ++i) {
        QString key = QStringLiteral("Table/OptionalCol_%1").arg(i);
        if (settings.contains(key)) {
            m_optionalVisible[i] = settings.value(key, s_defaultVisible[i]).toBool();
        } else {
            m_optionalVisible[i] = s_defaultVisible[i];
        }
    }
    beginResetModel();
    rebuildColumnMap();
    endResetModel();
}

int DataModel::totalVisibleColumns() const
{
    return m_visibleColumnMap.size();
}

int DataModel::logicalColumnAt(int visibleColumn) const
{
    if (visibleColumn < 0 || visibleColumn >= m_visibleColumnMap.size())
        return -1;
    return m_visibleColumnMap[visibleColumn];
}

//=== 状态辅助 ===

void DataModel::updateRowStatus(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;

    ParsedCacheData &d = m_rows[row];

    //空行判定：avid为0且路径都为空
    if (d.videoInfo.avid == 0 &&
        d.videoInfo.videoFilePath.isEmpty() &&
        d.videoInfo.audioFilePath.isEmpty() &&
        d.rowStatus != Exporting &&
        d.rowStatus != Completed) {
        d.rowStatus = Empty;
        return;
    }

    //有效/无效判定：基于isValid()
    if (d.rowStatus != Exporting && d.rowStatus != Completed) {
        d.rowStatus = d.videoInfo.isValid() ? Valid : Invalid;
    }
}

void DataModel::setExportProgress(int row, int percent)
{
    if (row < 0 || row >= m_rows.size())
        return;

    if (row >= m_progress.size())
        m_progress.resize(m_rows.size(), -1);

    m_progress[row] = percent;

    //同步状态
    if (percent >= 100) {
        m_rows[row].rowStatus = Completed;
    } else if (percent >= 0) {
        m_rows[row].rowStatus = Exporting;
    }

    //通知该行进度列更新
    //找到进度列在可见列中的位置
    for (int i = 0; i < m_visibleColumnMap.size(); ++i) {
        if (m_visibleColumnMap[i] == ColProgress) {
            QModelIndex idx = index(row, i);
            emit dataChanged(idx, idx);
            break;
        }
    }
}

//=== 私有辅助 ===

void DataModel::rebuildColumnMap()
{
    m_visibleColumnMap.clear();

    //强制列始终在前(0..7)
    for (int i = 0; i < MandatoryCount; ++i) {
        m_visibleColumnMap.append(i);
    }

    //可选列：仅可见的加入映射(逻辑列=8+OptionalColumn枚举值)
    for (int i = 0; i < OptionalCount; ++i) {
        if (m_optionalVisible[i]) {
            m_visibleColumnMap.append(MandatoryCount + i);
        }
    }
}

//=== 多P分组辅助 ===

bool DataModel::isFirstInGroup(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return true;

    //空行视为独立组
    if (m_rows[row].rowStatus == Empty)
        return true;

    qint64 avid = m_rows[row].videoInfo.avid;

    //avid为0视为独立行
    if (avid == 0)
        return true;

    //首行必然是组首
    if (row == 0)
        return true;

    //前一行如果avid不同(或前一行是空行)，则本行是组首
    qint64 prevAvid = m_rows[row - 1].videoInfo.avid;
    if (prevAvid != avid || m_rows[row - 1].rowStatus == Empty)
        return true;

    return false;
}

bool DataModel::isSharedColumn(int logicalCol) const
{
    //共享列：在多P同组中，首行正常显示、后续行显示↑的列
    //这些列的数据在多P中是相同的(标题、Up主等)
    if (logicalCol == ColTitle)
        return true;
    if (logicalCol >= MandatoryCount) {
        int optCol = logicalCol - MandatoryCount;
        if (optCol == ColOwnerName)
            return true;
    }
    return false;
}

QString DataModel::sharedColumnText(int row, int logicalCol) const
{
    if (row < 0 || row >= m_rows.size())
        return QString();

    const ParsedCacheData &d = m_rows[row];

    //空行返回空
    if (d.rowStatus == Empty)
        return QString();

    //如果是组首行，正常返回数据
    if (isFirstInGroup(row)) {
        if (logicalCol == ColTitle)
            return d.videoInfo.title;
        if (logicalCol >= MandatoryCount) {
            int optCol = logicalCol - MandatoryCount;
            if (optCol == ColOwnerName)
                return d.videoInfo.ownerName;
        }
        return QString();
    }

    //非首行：返回↑表示继承上一行同组数据
    return QStringLiteral("↑");
}
