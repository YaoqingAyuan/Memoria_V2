#ifndef CACHEFILEPARSER_H
#define CACHEFILEPARSER_H
//缓存(Cache)文件(File)解析器(Parser)类
/* AI建议自己读取函数与解析器函数(类分开)你问我读取函数哪里去了？
 * 人家是UI组件衍生出的槽函数，中后期搭建的 */

#include <QString>
#include <QDir>
#include <QJsonObject>
#include <QMap>

//初始容器类型QMap<QString, QString>,读取的entry.json&index.json文件信息进入该容器
typedef QMap<QString, QString> MetadataContainer;

//结构体：视频(Video)信息(Infor-mation)，读取的视频元数据存储到这里，以备其他模块调用
//★标为重要变量:人家出错，将会让主要功能“报废”(至于表格中其他的，空的时候填写“空”、找不到的时候填写“NULL”即可)
//其他的无关变量存储在容器rawMetadata中，大部分用不上,“右键-查看元数据”后才会“展示”
struct VideoInfo {
    qint64 avid;        //对应字段“avid”:缓存离线诊断ID（原始文件夹命名）
    QString bvid;       //对应字段“bvid”,视频网页(Av)Bv号
    QString title;      //★对应字段“owner_name”:视频标题
                        //(重要!读不到的话需用户UI界面自定义or调用默认哈希值标题)
    QString ownerName;  //对应字段“owner_name”:Up主昵称
    QString ownerId;    //对应字段“owner_id”:Up主账号UID
    QString coverUrl;   //对应字段“cover”:封面链接(应该用不着吧？)
    int videoQuality;   //对应字段“video_quality”:视频质量代码
                        //小心！如果4K等其他分辨率的代码不是数字，类型要改为QString
    QString qualityDescription;     //对应字段“quality_pithy_description”:质量描述
    qint64 totalTimeMilli;          //对应字段“total_time_milli”:视频总时长(单位:毫秒)
    qint64 totalBytes;              //对应字段“total_bytes”:视频总大小(单位:比特)
    qint64 downloadedBytes;         //对应字段“downloaded_bytes”:下载的比特数
    //还可以加上弹幕数量以及缓存时间

    //内部结构体结构声明
    struct PageData {       //Page_Data字段数据与ep(番剧)字段同构
        qint64 cid;         //PageData字段“cid”:视频Cid号(多P视频的标识)
                            //ep字段“episode_id”:ID标识
        int page;           //PageData字段“page”:P数(第几P视频)、ep字段字段“index”
        QString partTitle;  //PageData字段“part”:P标题(单P视频为自动生成lv_0_时间戳)
                            //ep字段“index_title”
        QString link;       //PageData字段“link”:站APP内部跳转链接，使用B站私有协议,用于APP内直接唤起视频播放页【感觉可以不要这个】
                            //ep字段同
        int width;          //PageData字段“width”:视频宽度、ep字段同
        int height;         //PageData字段“height”:视频高度、ep字段同
        int rotate;         //PageData字段“rotate”:旋转角度、ep字段同
    };
    PageData page_ep_Data;  //声明:VideoInfo里面有个PageData结构体

    QString cacheRootPath;  //缓存文件路径
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

//结构体：流(Stream)信息(Infor-mation),来自文件index.Json,音视频独立
struct StreamInfo {
    int id;             //对应字段“id”
    int bandwidth;      //对应字段“bandwidth”
    int codecid;        //对应字段“codecid”
    QString md5;        //对应字段“md5”:Md5值
    qint64 size;        //对应字段“size”:大小(单位:比特)
    QString FrameRate;  //对应字段“frame_rate”:帧率
    int width;          //对应字段“width”视频宽度(音频是0)
    int height;         //对应字段“height”视频高度(音频是0)
};

//类声明
class CacheFileParser
{
public:
    CacheFileParser();  //构造函数

    //成员函数们
    bool Cathe_Parse(const QString &folderPath, VideoInfo &outVideoInfo);
    bool findMediaFiles(const QString &dirPath, VideoInfo &info);
    qint64 getDirectorySize(const QDir &dir);
    //解析entry.json文件函数：从容器EntryJsonData读取数据填入VideoInfo结构体
    bool parseEntryJson(VideoInfo &info);
    //解析index.json文件函数：从容器IndexJsonData读取数据填入StreamInfo结构体
    bool parseIndexJson(VideoInfo &info, StreamInfo &videoStream, StreamInfo &audioStream);
    
    //UI(下拉菜单)访问接口函数:通过这个获取两私有容器数据(EntryJsonData、IndexJsonData,外部无法直接访问)
    //返回const引用，外部只能读取，不能修改
    //获取容器EntryJsonData中的entry.json文件信息
    const MetadataContainer& getEntryJsonData() const { return EntryJsonData; }
    //获取容器IndexJsonData中的index.json文件信息
    const MetadataContainer& getIndexJsonData() const { return IndexJsonData; }

private:
    //递归展平JSON函数(通用辅助函数)
    void flattenJsonRecursive(const QJsonObject &obj, const QString &prefix, MetadataContainer &container);
    //将entry.json文件中的字段，递归处理展开为一级字段(顺带去掉引号等特殊字符)
    bool EntryflattenJson(const QString &filePath);
    //将index.json文件中的字段，递归处理展开为一级字段(顺带去掉引号等特殊字符)
    bool indexflattenJson(const QString &filePath);


    //容器变量放入(私有成员变量),使每个解析器实例都有独立数据容器:支持并行解析多个文件夹、线程安全、测试友好
    //读取的entry.json文件信息进入该容器
    MetadataContainer EntryJsonData;
    //读取的index.json文件信息进入该容器
    MetadataContainer IndexJsonData;
};

#endif // CACHEFILEPARSER_H
/* 260720 概述：构建“日志”类，独立出去；准备重设工作流
 * 新问题：如何兼容番剧类型？
 * 依然存在的问题：
 * 1.结构体VideoInfo中的isValid() const函数的潜在Bug【测试了再修也不迟】
 * 2.考虑是否该把“关键数据”判定【判定为不完整不得入“任务队列”】
 * 3.摸清流信息结构体的作用？【确认与队列类（分配任务）相关，但细节未定】
 * 4.还有一个疑虑:当初将EntryJsonData与IndexJsonData放入私有变量中，就是为了使每个解析器实例都有独立数据容器
 * 然后VideoInfo与StreamInfo是否也应是一样的逻辑？
 * 经确认，我的疑虑是存在的：这意味着我需要将两容器重构到DatabaseManager类中
 * 解析器只负责解析“文件”然后填充“数据”类实例
 * 后续提供UI(渲染可视化表格)以及“队列类”(排队)-“FFmpeg类”(混流)调用
 *
 * 【已解决】缓存解析函数、两Json展平函数、~~~~解析函数三大模块需整理“工作流”
 * 制作简单的字符画思维导图；然后看看函数内部的运作机理
 * 【已解决】想办法让debug\warning\critical三个函数独立出去（毕竟其他模块也得用）
 * 后续看一看该模块内部各函数是什么意思？
 */

