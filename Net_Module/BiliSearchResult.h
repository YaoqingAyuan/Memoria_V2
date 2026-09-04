#ifndef BILISEARCHRESULT_H
#define BILISEARCHRESULT_H
//B站(Bili)搜索结果(SearchResult)数据结构
//供 BiliApiWorker 发射、VideoPlayback_Weight 接收展示

#include <QString>
#include <QList>
#include <QMetaType>

//分P信息结构：对应B站 view 接口返回的 pages 数组中的每一项
struct BiliPageInfo {
    qint64 cid = 0;         //分P的CID(用于获取播放地址)
    int page = 0;           //P数(第几P)
    QString part;           //分P标题
    qint64 duration = 0;    //时长(秒)
    int width = 0;          //视频宽度
    int height = 0;         //视频高度
};

struct BiliSearchResult {
    QString title;          //视频标题
    qint64 avid = 0;        //AV号
    QString bvid;           //BV号
    qint64 cid = 0;         //第1P的CID(兼容旧逻辑)
    QString coverUrl;       //封面链接
    QString ownerName;      //UP主昵称
    qint64 duration = 0;    //总时长(秒)
    qint64 playCount = 0;   //播放量
    bool isAvailable = true; //是否在架(true=正常可访问, false=已下架/不可见)
    QList<BiliPageInfo> pages;  //分P列表(view接口返回，search接口为空)

    //格式化时长为 mm:ss 或 h:mm:ss
    QString formattedDuration() const {
        int h = duration / 3600;
        int m = (duration % 3600) / 60;
        int s = duration % 60;
        if (h > 0)
            return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }

    //格式化播放量(万/亿)
    QString formattedPlayCount() const {
        if (playCount >= 100000000)
            return QStringLiteral("%1亿").arg(playCount / 100000000.0, 0, 'f', 1);
        if (playCount >= 10000)
            return QStringLiteral("%1万").arg(playCount / 10000.0, 0, 'f', 1);
        return QString::number(playCount);
    }
};

Q_DECLARE_METATYPE(BiliSearchResult)

#endif // BILISEARCHRESULT_H
