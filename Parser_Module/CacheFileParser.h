#ifndef CACHEFILEPARSER_H
#define CACHEFILEPARSER_H
//缓存(Cache)文件(File)解析器(Parser)类
/* AI建议自己读取函数与解析器函数(类分开)你问我读取函数哪里去了？
 * 人家是UI组件衍生出的槽函数，中后期搭建的 */

#include <QString>
#include <QDir>
#include <QJsonObject>
#include <QList>
#include "Core/ParsedCacheData.h"

//类声明
class CacheFileParser
{
public:
    CacheFileParser();  //构造函数

    //成员函数们
    //解析一个离线诊断ID文件夹，遍历所有子目录(多P场景)，每个子目录生成一个ParsedCacheData
    //folderPath=离线诊断ID文件夹路径，outDataList=解析结果列表
    //返回true表示至少成功解析了一个子目录
    bool Cathe_Parse(const QString &folderPath, QList<ParsedCacheData> &outDataList);
    bool findMediaFiles(const QString &dirPath, ParsedCacheData &data);
    qint64 getDirectorySize(const QDir &dir);
    //解析entry.json文件函数：从QJsonObject直接读取数据填入VideoInfo结构体
    bool parseEntryJson(const QJsonObject &obj, VideoInfo &info);
    //解析index.json文件函数：从QJsonObject直接读取数据填入StreamInfo结构体
    bool parseIndexJson(const QJsonObject &obj, StreamInfo &videoStream, StreamInfo &audioStream);

private:
    //读取JSON文件并解析为QJsonObject(替代原展平中间步骤，直接解析)
    bool loadJsonObject(const QString &filePath, QJsonObject &outObj);
    //解析单个子目录(离线诊断ID下的一个c_xxx或番剧集号目录)
    //subDirPath=子目录路径，cacheRootPath=离线诊断ID根路径(用于cacheRootPath字段)
    bool parseSingleSubDir(const QString &subDirPath, const QString &cacheRootPath, ParsedCacheData &outData);
};

#endif // CACHEFILEPARSER_H
/* 260729 概述：将两结构体的声明重构到ParsedCacheData类中，作为该类成员变量
 * 【优化】移除EntryJsonData与IndexJsonData中间容器，解析器直接从QJsonObject读取数据填充结构体
 * 减少内存占用(不再保存完整JSON展平数据)，提升解析速度(省去递归展平步骤)
 * 新问题：如何兼容番剧类型？
 * 依然存在的问题：
 * 1.结构体VideoInfo中的isValid() const函数的潜在Bug【测试了再修也不迟】
 * 2.考虑是否该把"关键数据"判定【判定为不完整不得入"任务队列"】
 *
 * 【已解决】摸清流信息结构体的作用
 * 【已解决】解析器只负责解析"文件"然后填充"数据"类实例
 * 后续提供UI(渲染可视化表格)以及"队列类"(排队)-"FFmpeg类"(混流)调用
 * 【已解决】缓存解析函数、解析函数两大模块工作流已整理(字符画思维导图)
 * 【已解决】想办法让debug\warning\critical三个函数独立出去（毕竟其他模块也得用）
 * 后续看一看该模块内部各函数是什么意思？
 */
