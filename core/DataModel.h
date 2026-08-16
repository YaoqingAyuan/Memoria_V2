#ifndef DATAMODEL_H
#define DATAMODEL_H
//数据(Data)模型(Model)类：QAbstractTableModel子类
//管理主窗口表格的数据层：行/列定义、多P分组显示、状态标记、列可见性

#include <QAbstractTableModel>
#include <QVector>
#include <QSettings>
#include "ParsedCacheData.h"

class DataModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    //=== 列定义 ===
    //强制列(始终显示，不出现在设置UI中)
    enum MandatoryColumn {
        ColSeq = 0,         //序号(虚拟行索引，不存储在数据中)
        ColStatus,          //状态(RowStatus的中文显示)
        ColTitle,           //标题(VideoInfo.title)
        ColPageNo,          //分P号(PageData.page / ep.index)
        ColPartTitle,       //分P标题(PageData.partTitle / ep.index_title)
        ColVideoPath,       //视频路径(VideoInfo.videoFilePath)
        ColAudioPath,       //音频路径(VideoInfo.audioFilePath)
        ColProgress,        //进度(导出进度百分比)
        MandatoryCount      //强制列总数=8
    };

    //可选列(可在设置中勾选控制显隐)
    enum OptionalColumn {
        ColAvid = 0,            //离线ID(VideoInfo.avid)
        ColBvid,                //Bv号(VideoInfo.bvid)
        ColVideoType,           //视频类型(Normal/Bangumi中文显示)
        ColOwnerName,           //Up主(VideoInfo.ownerName)
        ColOwnerId,             //Up主UID(VideoInfo.ownerId)
        ColCreateTime,          //创建时间(VideoInfo.create_timestamp转可读格式)
        ColDuration,            //时长(VideoInfo.totalTimeMilli转秒)
        ColFileSize,            //文件大小(VideoInfo.totalBytes转MB)
        ColDanmakuCount,        //弹幕数(VideoInfo.recent_danmaku_count)
        ColResolution,          //分辨率(videoStream.width×height)
        OptionalCount           //可选列总数=10
    };

    explicit DataModel(QObject *parent = nullptr);

    //=== QAbstractTableModel 必须实现的虚函数 ===
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    //=== 行操作 ===
    //在末尾添加一个空行(默认构造的ParsedCacheData)，返回新行索引
    int addEmptyRow();
    //在指定位置插入一个空行
    void insertEmptyRow(int row);
    //删除指定行
    void removeRow(int row);
    //用解析数据替换指定行(若row=-1则追加到末尾)
    void setRowData(int row, const ParsedCacheData &data);
    //获取指定行的数据引用(只读)
    const ParsedCacheData& getRowData(int row) const;
    //获取指定行的数据(可写，修改后需调用refreshRow)
    ParsedCacheData& getRowDataRef(int row);
    //刷新指定行(数据修改后通知视图更新)
    void refreshRow(int row);
    //清空所有行
    void clear();

    //=== 列可见性 ===
    //获取可选列的可见状态
    bool isOptionalColumnVisible(OptionalColumn col) const;
    //设置可选列的可见状态，并保存到配置
    void setOptionalColumnVisible(OptionalColumn col, bool visible);
    //从配置加载列可见性
    void loadColumnVisibility();
    //获取当前实际显示的总列数(强制列+可见的可选列)
    int totalVisibleColumns() const;
    //根据视图列索引(0..totalVisibleColumns-1)获取对应的逻辑列标识
    //返回值：0..7为强制列(MandatoryColumn)，8..17为可选列(OptionalColumn+8)
    //返回-1表示无效
    int logicalColumnAt(int visibleColumn) const;

    //=== 状态辅助 ===
    //根据ParsedCacheData的内容自动判定并设置rowStatus
    void updateRowStatus(int row);
    //设置导出进度(0-100)
    void setExportProgress(int row, int percent);
    //标记导出失败：清空进度，状态回退为有效/无效(允许重试)
    void markExportFailed(int row);

private:
    //数据存储：每行一个ParsedCacheData
    QVector<ParsedCacheData> m_rows;

    //导出进度(按行索引存储，-1表示无进度)
    QVector<int> m_progress;

    //可选列可见性配置(数组索引对应OptionalColumn枚举)
    bool m_optionalVisible[OptionalCount];

    //默认的可选列可见性(首次启动时的默认值)
    //默认显示：离线ID、Up主、分辨率，其余隐藏
    static const bool s_defaultVisible[OptionalCount];

    //构建当前可见列的映射表：visibleIndex → logicalColumn
    //logicalColumn: 0..7=强制列, 8..17=可选列(8+OptionalColumn枚举值)
    QVector<int> m_visibleColumnMap;

    //重建可见列映射表(在列可见性变化时调用)
    void rebuildColumnMap();

    //=== 多P分组辅助 ===
    //判断指定行是否是同AVID组的首行
    //首行=该行AVID非0，且前一行AVID与本行不同(或前一行AVID为0)
    bool isFirstInGroup(int row) const;
    //判断指定列是否是"共享列"(多P同组中首行填充、后续行显示↑的列)
    bool isSharedColumn(int logicalCol) const;
    //获取共享列的显示文本(首行正常返回，非首行返回"↑")
    QString sharedColumnText(int row, int logicalCol) const;
};

#endif // DATAMODEL_H
