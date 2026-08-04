#ifndef CACHEFILEPARSER_H
#define CACHEFILEPARSER_H
//缓存(Cache)文件(File)解析器(Parser)类
/* AI建议自己读取函数与解析器函数(类分开)你问我读取函数哪里去了？
 * 人家是UI组件衍生出的槽函数，中后期搭建的 */

#include <QString>
#include <QDir>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include "ParsedCacheData.h"

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
    //解析entry.json文件函数：从容器EntryJsonData读取数据填入VideoInfo结构体
    bool parseEntryJson(ParsedCacheData &data);
    //解析index.json文件函数：从容器IndexJsonData读取数据填入StreamInfo结构体
    bool parseIndexJson(ParsedCacheData &data);

private:
    //递归展平JSON函数(通用辅助函数)
    void flattenJsonRecursive(const QJsonObject &obj, const QString &prefix, MetadataContainer &container);
    //将entry.json文件中的字段，递归处理展开为一级字段(顺带去掉引号等特殊字符)
    bool EntryflattenJson(const QString &filePath, MetadataContainer &container);
    //将index.json文件中的字段，递归处理展开为一级字段(顺带去掉引号等特殊字符)
    bool indexflattenJson(const QString &filePath, MetadataContainer &container);
    //解析单个子目录(离线诊断ID下的一个c_xxx或番剧集号目录)
    //subDirPath=子目录路径，cacheRootPath=离线诊断ID根路径(用于cacheRootPath字段)
    bool parseSingleSubDir(const QString &subDirPath, const QString &cacheRootPath, ParsedCacheData &outData);
};

#endif // CACHEFILEPARSER_H
/* 260729 概述：将两结构体的声明重构到ParsedCacheData类中，作为该类成员变量
 * 新问题：如何兼容番剧类型？
 * 依然存在的问题：
 * 1.结构体VideoInfo中的isValid() const函数的潜在Bug【测试了再修也不迟】
 * 2.考虑是否该把"关键数据"判定【判定为不完整不得入"任务队列"】
 *
 * 【已解决】摸清流信息结构体的作用
 * 【已解决】还有一个疑虑:当初将EntryJsonData与IndexJsonData放入私有变量中，就是为了使每个解析器实例都有独立数据容器
 * 将两容器重构到ParsedCacheData类中，解析器只负责解析"文件"然后填充"数据"类实例
 * 后续提供UI(渲染可视化表格)以及"队列类"(排队)-"FFmpeg类"(混流)调用
 * 【已解决】缓存解析函数、两Json展平函数、~~~~解析函数三大模块需整理"工作流"
 * 制作简单的字符画思维导图；然后看看函数内部的运作机理
 * 【已解决】想办法让debug\warning\critical三个函数独立出去（毕竟其他模块也得用）
 * 后续看一看该模块内部各函数是什么意思？
 */
