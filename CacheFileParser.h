#ifndef CACHEFILEPARSER_H
#define CACHEFILEPARSER_H
//缓存(Cache)文件(File)解析器(Parser)类
/* AI建议自己读取函数与解析器函数(类分开)你问我读取函数哪里去了？
 * 人家是UI组件衍生出的槽函数，中后期搭建的 */

#include <QString>
#include <QDir>
#include <QJsonObject>

//结构体：视频(Video)信息(Infor-mation)，读取的视频元数据存储到这里，以备其他模块调用
//★标为重要变量:人家出错，将会让主要功能“报废”(至于表格中其他的，空的时候填写“空”、找不到的时候填写“NULL”即可)
//其他的无关变量存储在另一个“初级容器”中，大部分用不上，
//估计也就让少量发烧友“右键-查看元数据”后才会“展示”出来
struct VideoInfo {
    qint64 avid;        //视频Av号【Bv号还会绑Av？】
    QString bvid;       //视频Bv号
    QString title;      //★视频标题(重要,命名输出文件)
    QString ownerName;  //Up主昵称
    QString coverUrl;   //封面链接(应该用不着吧？)
    int videoQuality;   //视频质量
    QString qualityDescription;     //质量描述？
    qint64 totalTimeMilli;          //视频总时间？
    qint64 totalBytes;              //视频总大小(估计)
    qint64 downloadedBytes;         //下载的比特数

    //内部结构体结构声明
    struct PageData {       //Page字段数据
        qint64 cid;         //视频Cid号(多P视频的标识)
        int page;           //P数(第几P视频)
        QString partTitle;  //P标题
        QString link;       //视频的链接？【应该是Page字段的】
        int width;          //视频宽度
        int height;         //视频高度
        int rotate;         //旋转角度
    };
    PageData pageData;      //声明:VideoInfo里面有个PageData结构体

    QString cacheRootPath;  //缓存root路径？
    QString entryJsonPath;  //★entry.Json文件路径
    QString indexJsonPath;  //index.Json文件路径
    QString videoFilePath;  //★视频文件路径
    QString audioFilePath;  //★音频文件路径

    //(布尔型)函数：(路径)是否有效-返回不可修改
    bool isValid() const {
        //当videoFilePath与audioFilePath同时非空，才会返回True；否则返回False
        return !videoFilePath.isEmpty() && !audioFilePath.isEmpty();
    }
    /*该函数潜在逻辑问题：
     * 1.目前函数只检查了字符串是否有内容，并没有检查路径是否真的存在或格式是否正确
     * (按照经验，这两个监测应该分离，在某个阶段统一判定)
     * 2.isEmpty()只看"有没有字符"，不关心是不是 null。所以 QString()和 QString("")对 isEmpty()来说没区别，都是 true。
     * 如果字符串里只有空白字符（空格、\t、\n等），isEmpty()照样返回 false，因为长度 > 0。
     * 要判"视觉上空"得同时使用.trim()后再判定(顺便去掉首尾空格，防止用户输入了空格)
     * 【有一说一，到时候这两个变量的写入，是用户在UI上点击“文件夹图标”后，系统自动写入，这应该不是问题】
     */
};

//结构体：流(Stream)信息(Infor-mation)
struct StreamInfo {
    int id;             //视频ID
    int bandwidth;      //带宽？
    int codecid;        //视频Cid号
    QString md5;        //Md5值
    qint64 size;        //大小
    QString FrameRate;  //帧率
    int width;          //视频宽度
    int height;         //视频高度
};

//类声明
class CacheFileParser
{
public:
    CacheFileParser();  //构造函数

    //成员函数们
    bool parse(const QString &folderPath, VideoInfo &outVideoInfo);
    bool parseEntryJson(const QDir &dir, VideoInfo &info);
    bool parseIndexJson(const QDir &dir, VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream);
    bool findMediaFiles(const QDir &dir, const QString &qualityDir, VideoInfo &info);
    qint64 getDirectorySize(const QDir &dir);

private:
    void logDebug(const QString &msg);
    void logWarning(const QString &msg);
    void logCritical(const QString &msg);
};

#endif // CACHEFILEPARSER_H

/* 260716“标准化”全部样本数据，准备修正该模块！(真坐牢！！！)
 * 1.结构体VideoInfo中的isValid() const函数的潜在Bug
 * 2.摸清结构体VideoInfo中的变量都对应了元数据文件的哪个部分？
 * 目标：保留核心(如标题)、显示稳固(如分辨率、大小估测)、不管其他(如番剧季度ID)
 * 3.思考读取文件({字段})→标准化为“换行字段”(为“直接查看元数据”功能准备)
 * →根据所需结构体关键字段(关联)→UI的元数据表格(反正重要的仅仅只有“标题”，剩下的仅供参考)
 * 这一工作流的可能性（有必要的话增加函数）
 * 【明日2-3-4一起解决！】
 * 4.摸清流信息结构体的作用？【队列类（分配任务）相关？】
 * 6.想办法让debug\warning\critical三个函数独立出去（毕竟其他模块也得用）  */
