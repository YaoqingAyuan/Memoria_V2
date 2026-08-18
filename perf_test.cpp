#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
#include "Parser_Module/CacheFileParser.h"
#include "Core/ParsedCacheData.h"

//直接输出到stderr(避免qDebug被Release配置吞掉)
#define LOG(...) do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)

//生成模拟B站缓存数据(创建entry.json/index.json/video.m4s/audio.m4s)
static void createTestCache(const QString &basePath, int count)
{
    QDir().mkpath(basePath);
    for (int i = 0; i < count; ++i) {
        QString subDir = basePath + QString("/c_%1").arg(i, 6, 10, QChar('0'));
        QString qualityDir = subDir + "/q80";
        QDir().mkpath(qualityDir);

        //entry.json
        QJsonObject entry;
        entry["avid"] = 100000 + i;
        entry["bvid"] = QString("BV1xx%1").arg(i);
        entry["title"] = QString("测试视频标题_%1").arg(i);
        entry["owner_name"] = "测试UP主";
        entry["owner_id"] = "12345";
        entry["cover"] = "http://example.com/cover.jpg";
        entry["video_quality"] = 80;
        entry["quality_pithy_description"] = "高清 1080P";
        entry["total_time_milli"] = 600000;
        entry["time_create_stamp"] = 1700000000000LL;
        entry["danmaku_count"] = 500;

        QJsonObject pageData;
        pageData["cid"] = 200000 + i;
        pageData["page"] = i + 1;
        pageData["part"] = QString("第%1P").arg(i + 1);
        pageData["link"] = "bili://xxx";
        pageData["width"] = 1920;
        pageData["height"] = 1080;
        pageData["rotate"] = 0;
        entry["page_data"] = pageData;

        QFile entryFile(subDir + "/entry.json");
        entryFile.open(QIODevice::WriteOnly);
        entryFile.write(QJsonDocument(entry).toJson());
        entryFile.close();

        //index.json
        QJsonObject index;
        QJsonObject vs;
        vs["id"] = 1;
        vs["bandwidth"] = 5000000;
        vs["codecid"] = 7;
        vs["md5"] = "abcdef1234567890";
        vs["size"] = 100000000;
        vs["frame_rate"] = "60";
        vs["width"] = 1920;
        vs["height"] = 1080;
        QJsonArray va; va.append(vs);
        index["video"] = va;

        QJsonObject as;
        as["id"] = 2;
        as["bandwidth"] = 128000;
        as["codecid"] = 13;
        as["md5"] = "fedcba0987654321";
        as["size"] = 5000000;
        QJsonArray aa; aa.append(as);
        index["audio"] = aa;

        QFile indexFile(qualityDir + "/index.json");
        indexFile.open(QIODevice::WriteOnly);
        indexFile.write(QJsonDocument(index).toJson());
        indexFile.close();

        //video.m4s / audio.m4s (空文件占位)
        QFile(qualityDir + "/video.m4s").open(QIODevice::WriteOnly);
        QFile(qualityDir + "/audio.m4s").open(QIODevice::WriteOnly);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString basePath = QCoreApplication::applicationDirPath() + "/test_cache";

    //创建测试数据(100个子目录)
    LOG("生成测试缓存数据(100条)...\n");
    createTestCache(basePath, 100);

    //第一轮解析(冷启动)
    LOG("\n=== 第1轮解析(冷启动) ===\n");
    QElapsedTimer t1;
    t1.start();
    CacheFileParser parser1;
    QList<ParsedCacheData> list1;
    parser1.Cathe_Parse(basePath, list1);
    qint64 ms1 = t1.elapsed();
    double avg1 = list1.isEmpty() ? 0.0 : (double)ms1 / list1.size();
    LOG("解析条数: %d  耗时: %lld ms  平均: %.3f ms/条\n",
        list1.size(), (long long)ms1, avg1);

    //第二轮解析(热启动)
    LOG("\n=== 第2轮解析(热启动) ===\n");
    QElapsedTimer t2;
    t2.start();
    CacheFileParser parser2;
    QList<ParsedCacheData> list2;
    parser2.Cathe_Parse(basePath, list2);
    qint64 ms2 = t2.elapsed();
    double avg2 = list2.isEmpty() ? 0.0 : (double)ms2 / list2.size();
    LOG("解析条数: %d  耗时: %lld ms  平均: %.3f ms/条\n",
        list2.size(), (long long)ms2, avg2);

    //第三轮解析(验证稳定性)
    LOG("\n=== 第3轮解析(稳定性验证) ===\n");
    QElapsedTimer t3;
    t3.start();
    CacheFileParser parser3;
    QList<ParsedCacheData> list3;
    parser3.Cathe_Parse(basePath, list3);
    qint64 ms3 = t3.elapsed();
    double avg3 = list3.isEmpty() ? 0.0 : (double)ms3 / list3.size();
    LOG("解析条数: %d  耗时: %lld ms  平均: %.3f ms/条\n",
        list3.size(), (long long)ms3, avg3);

    //验证数据正确性
    LOG("\n=== 数据正确性验证 ===\n");
    if (!list1.isEmpty()) {
        const auto &d = list1[0];
        LOG("标题: %s  AVID: %lld  BVID: %s  分辨率: %dx%d  类型: %d\n",
            d.videoInfo.title.toUtf8().constData(),
            (long long)d.videoInfo.avid,
            d.videoInfo.bvid.toUtf8().constData(),
            d.videoStream.width, d.videoStream.height,
            d.videoInfo.videoType);
    }

    //内存占用估算
    LOG("\n=== 内存优化评估 ===\n");
    LOG("优化前: 每条ParsedCacheData含2个MetadataContainer(QMap<QString,QString>)\n");
    LOG("  每个entry.json展平约100+字段, index.json约15字段\n");
    LOG("  QMap节点开销: 每节点约32字节 + QString键/值各16+字节\n");
    LOG("  每条估算额外内存: ~15-30KB (取决于JSON字段数量)\n");
    LOG("优化后: 直接从QJsonObject解析, 无中间容器\n");
    LOG("  每条ParsedCacheData仅含VideoInfo + 2个StreamInfo\n");
    LOG("  省去了: 递归展平步骤 + 100+个QString键值对的内存分配\n");

    //清理测试数据
    QDir(basePath).removeRecursively();
    LOG("\n测试完成，已清理测试数据。\n");

    return 0;
}
