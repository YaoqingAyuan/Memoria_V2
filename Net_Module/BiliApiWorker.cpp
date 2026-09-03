#include "BiliApiWorker.h"
#include "Core/logger.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QNetworkCookie>
#include <QUrlQuery>

BiliApiWorker::BiliApiWorker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    loadCookie();
    Logger::instance()->debug("BiliApi", QString("BiliApiWorker 已创建, 登录状态: %1")
        .arg(isLoggedIn() ? "已登录" : "未登录"));
}

BiliApiWorker::~BiliApiWorker()
{
    Logger::instance()->debug("BiliApi", "BiliApiWorker 已销毁");
}

QNetworkRequest BiliApiWorker::createRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    //B站API需要的通用Header
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    request.setRawHeader("Referer", "https://www.bilibili.com");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    if (!m_cookie.isEmpty())
        request.setRawHeader("Cookie", m_cookie.toUtf8());
    return request;
}

// ========== 搜索 ==========

void BiliApiWorker::searchByKeyword(const QString &keyword, int page)
{
    QUrl url("https://api.bilibili.com/x/web-interface/wbi/search/type");
    QUrlQuery query;
    query.addQueryItem("search_type", "video");
    query.addQueryItem("keyword", keyword);
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("page_size", "20");
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit searchFailed(reply->errorString());
            return;
        }
        QList<BiliSearchResult> results = parseSearchResults(reply->readAll());
        emit searchResultReady(results);
    });
}

void BiliApiWorker::searchByBvid(const QString &bvid)
{
    //用view接口查询BV号信息(同时用于下架检测)
    QUrl url("https://api.bilibili.com/x/web-interface/view");
    QUrlQuery query;
    query.addQueryItem("bvid", bvid);
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, bvid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit searchFailed(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        int code = root.value("code").toInt();

        if (code != 0) {
            //错误码 -404/"稿件不可见" 或 -400/"请求错误" 均视为不可访问
            QString message = root.value("message").toString();
            emit availabilityChecked(bvid, false, message);
            return;
        }

        BiliSearchResult result = parseVideoInfo(data);
        QList<BiliSearchResult> list;
        list.append(result);
        emit searchResultReady(list);
        emit availabilityChecked(bvid, true, QStringLiteral("视频正常在线"));
    });
}

void BiliApiWorker::searchByAvid(qint64 avid)
{
    //bvid为空时用avid查询(view API支持aid参数)
    QUrl url("https://api.bilibili.com/x/web-interface/view");
    QUrlQuery query;
    query.addQueryItem("aid", QString::number(avid));
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, avid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit searchFailed(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        int code = root.value("code").toInt();

        if (code != 0) {
            QString message = root.value("message").toString();
            //bvid未知，用avid构造标识
            emit availabilityChecked(QString::number(avid), false, message);
            return;
        }

        BiliSearchResult result = parseVideoInfo(data);
        QList<BiliSearchResult> list;
        list.append(result);
        //用API返回的真实bvid发出下架检测信号
        emit searchResultReady(list);
        emit availabilityChecked(result.bvid, true, QStringLiteral("视频正常在线"));
    });
}

// ========== 下架检测 ==========

void BiliApiWorker::checkAvailability(const QString &bvid)
{
    //searchByBvid 内部已经完成了下架检测(view接口返回code即判定)
    searchByBvid(bvid);
}

// ========== 获取播放地址 ==========

void BiliApiWorker::fetchPlayUrl(const QString &bvid, int cid, int quality)
{
    QUrl url("https://api.bilibili.com/x/player/playurl");
    QUrlQuery query;
    query.addQueryItem("bvid", bvid);
    query.addQueryItem("cid", QString::number(cid));
    query.addQueryItem("qn", QString::number(quality));
    query.addQueryItem("fnval", "0");   //仅MP4格式(非DASH)，便于QMediaPlayer直接播放
    query.addQueryItem("fnver", "0");
    query.addQueryItem("fourk", "0");
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, bvid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::instance()->warning("BiliApi", QString("播放地址请求失败: %1").arg(reply->errorString()));
            emit playUrlFailed(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root.value("code").toInt();
        if (code != 0) {
            QString errMsg = root.value("message").toString();
            Logger::instance()->warning("BiliApi", QString("播放地址API返回错误: code=%1, msg=%2").arg(code).arg(errMsg));
            emit playUrlFailed(errMsg);
            return;
        }

        QJsonObject data = root.value("data").toObject();
        QJsonArray durl = data.value("durl").toArray();
        if (durl.isEmpty()) {
            Logger::instance()->warning("BiliApi", "播放地址durl为空(fnval=0可能不支持此视频)");
            emit playUrlFailed(QStringLiteral("未获取到播放地址(可能需要登录)"));
            return;
        }

        //取第一个流地址
        QString playUrl = durl.at(0).toObject().value("url").toString();
        if (playUrl.startsWith("//"))
            playUrl = "https:" + playUrl;

        Logger::instance()->debug("BiliApi", QString("获取播放地址成功: %1").arg(bvid));
        emit playUrlReady(bvid, playUrl);
    });
}

void BiliApiWorker::fetchPlayUrlDash(const QString &bvid, int cid, int quality)
{
    QUrl url("https://api.bilibili.com/x/player/playurl");
    QUrlQuery query;
    query.addQueryItem("bvid", bvid);
    query.addQueryItem("cid", QString::number(cid));
    query.addQueryItem("qn", QString::number(quality));
    query.addQueryItem("fnval", "4048");  //DASH+HDR+4K+Dolby+8K+AV1
    query.addQueryItem("fnver", "0");
    query.addQueryItem("fourk", "1");      //允许4K
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, bvid]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::instance()->warning("BiliApi", QString("DASH播放地址请求失败: %1").arg(reply->errorString()));
            emit playUrlFailed(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root.value("code").toInt();
        if (code != 0) {
            QString errMsg = root.value("message").toString();
            Logger::instance()->warning("BiliApi", QString("DASH播放地址API错误: code=%1, msg=%2").arg(code).arg(errMsg));
            emit playUrlFailed(errMsg);
            return;
        }

        QJsonObject data = root.value("data").toObject();
        QJsonObject dash = data.value("dash").toObject();
        QJsonArray videoArr = dash.value("video").toArray();
        QJsonArray audioArr = dash.value("audio").toArray();

        if (videoArr.isEmpty()) {
            Logger::instance()->warning("BiliApi", "DASH视频流为空");
            emit playUrlFailed(QStringLiteral("DASH视频流为空"));
            return;
        }

        //选择最高画质的H.264视频流(codecid=7)，无H.264则取最高画质任意编码
        QJsonObject bestVideo;
        int bestVideoId = -1;
        QJsonObject bestH264Video;
        int bestH264Id = -1;
        for (const QJsonValue &v : videoArr) {
            QJsonObject obj = v.toObject();
            int qid = obj.value("id").toInt();
            int codecid = obj.value("codecid").toInt();
            if (qid > bestVideoId) {
                bestVideoId = qid;
                bestVideo = obj;
            }
            if (codecid == 7 && qid > bestH264Id) {
                bestH264Id = qid;
                bestH264Video = obj;
            }
        }
        QJsonObject videoObj = !bestH264Video.isEmpty() ? bestH264Video : bestVideo;

        //选择最高码率的音频流
        QJsonObject bestAudio;
        int bestAudioBw = -1;
        for (const QJsonValue &a : audioArr) {
            QJsonObject obj = a.toObject();
            int bw = obj.value("bandwidth").toInt();
            if (bw > bestAudioBw) {
                bestAudioBw = bw;
                bestAudio = obj;
            }
        }

        QString videoUrl = videoObj.value("baseUrl").toString();
        if (videoUrl.isEmpty())
            videoUrl = videoObj.value("base_url").toString();
        if (videoUrl.startsWith("//"))
            videoUrl = "https:" + videoUrl;

        QString audioUrl;
        if (!bestAudio.isEmpty()) {
            audioUrl = bestAudio.value("baseUrl").toString();
            if (audioUrl.isEmpty())
                audioUrl = bestAudio.value("base_url").toString();
            if (audioUrl.startsWith("//"))
                audioUrl = "https:" + audioUrl;
        }

        if (videoUrl.isEmpty()) {
            emit playUrlFailed(QStringLiteral("无法解析DASH视频URL"));
            return;
        }

        int actualQn = data.value("quality").toInt();
        Logger::instance()->debug("BiliApi",
            QString("DASH播放地址成功: %1, 实际画质qn=%2, 视频qn=%3")
                .arg(bvid).arg(actualQn).arg(videoObj.value("id").toInt()));
        emit playUrlDashReady(bvid, videoUrl, audioUrl);
    });
}

// ========== 二维码登录 ==========

void BiliApiWorker::requestLoginQrCode()
{
    QUrl url("https://passport.bilibili.com/x/passport-login/web/qrcode/generate");

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::instance()->warning("BiliApi",
                QString("获取二维码失败: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonObject data = root.value("data").toObject();

        QString qrUrl = data.value("url").toString();
        QString qrcodeKey = data.value("qrcode_key").toString();

        Logger::instance()->debug("BiliApi", "二维码获取成功，等待扫码");
        emit qrCodeReady(qrUrl, qrcodeKey);
    });
}

void BiliApiWorker::pollLoginStatus(const QString &qrcodeKey)
{
    QUrl url("https://passport.bilibili.com/x/passport-login/web/qrcode/poll");
    QUrlQuery query;
    query.addQueryItem("qrcode_key", qrcodeKey);
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(createRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QJsonObject data = root.value("data").toObject();
        int code = data.value("code").toInt();
        QString message = data.value("message").toString();

        QString newCookie;
        if (code == 0) {
            //登录成功，用QNetworkCookie正确解析Set-Cookie头
            //reply->rawHeader()在Qt6中用\n拼接多个Set-Cookie，含换行符和Path/Domain等属性
            //直接使用会导致后续请求HTTP头含非法字符
            QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(reply->rawHeader("Set-Cookie"));
            QString sessdata, biliJct;
            for (const auto &cookie : cookies) {
                if (cookie.name() == "SESSDATA")
                    sessdata = QString::fromUtf8(cookie.value());
                else if (cookie.name() == "bili_jct")
                    biliJct = QString::fromUtf8(cookie.value());
            }
            if (!sessdata.isEmpty())
                newCookie = QStringLiteral("SESSDATA=%1; bili_jct=%2").arg(sessdata, biliJct);
            m_cookie = newCookie;
            saveCookie();
            Logger::instance()->debug("BiliApi", "B站登录成功");
        }

        emit loginStatusChanged(code, message, newCookie);
    });
}

// ========== Cookie管理 ==========

void BiliApiWorker::setCookie(const QString &cookie)
{
    m_cookie = cookie;
    saveCookie();
}

void BiliApiWorker::clearCookie()
{
    m_cookie.clear();
    QSettings settings;
    settings.remove("bili/cookie");
    Logger::instance()->debug("BiliApi", "已清除B站登录Cookie");
}

void BiliApiWorker::loadCookie()
{
    QSettings settings;
    m_cookie = settings.value("bili/cookie").toString();
}

void BiliApiWorker::saveCookie()
{
    QSettings settings;
    settings.setValue("bili/cookie", m_cookie);
}

// ========== JSON解析 ==========

QList<BiliSearchResult> BiliApiWorker::parseSearchResults(const QByteArray &data)
{
    QList<BiliSearchResult> results;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    int code = root.value("code").toInt();
    if (code != 0) {
        Logger::instance()->warning("BiliApi",
            QString("搜索失败: %1").arg(root.value("message").toString()));
        return results;
    }

    QJsonObject dataObj = root.value("data").toObject();
    QJsonArray resultArray = dataObj.value("result").toArray();

    for (const QJsonValue &val : resultArray) {
        QJsonObject item = val.toObject();
        BiliSearchResult r;
        r.title = item.value("title").toString()
            .replace("<em class=\"keyword\">", "")
            .replace("</em>", "");  //去除搜索结果高亮标签
        r.avid = item.value("aid").toVariant().toLongLong();
        r.bvid = item.value("bvid").toString();
        r.coverUrl = item.value("pic").toString();
        if (r.coverUrl.startsWith("//"))
            r.coverUrl = "https:" + r.coverUrl;
        r.ownerName = item.value("author").toString();
        r.duration = item.value("duration").toVariant().toLongLong();
        r.playCount = item.value("play").toVariant().toLongLong();
        r.isAvailable = true;
        results.append(r);
    }

    Logger::instance()->debug("BiliApi",
        QString("搜索解析完成，共 %1 条结果").arg(results.size()));
    return results;
}

BiliSearchResult BiliApiWorker::parseVideoInfo(const QByteArray &data)
{
    BiliSearchResult r;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonObject dataObj = root.value("data").toObject();

    r.title = dataObj.value("title").toString();
    r.avid = dataObj.value("aid").toVariant().toLongLong();
    r.bvid = dataObj.value("bvid").toString();
    r.cid = dataObj.value("cid").toVariant().toLongLong();
    r.coverUrl = dataObj.value("pic").toString();
    if (r.coverUrl.startsWith("//"))
        r.coverUrl = "https:" + r.coverUrl;
    QJsonObject owner = dataObj.value("owner").toObject();
    r.ownerName = owner.value("name").toString();
    r.duration = dataObj.value("duration").toVariant().toLongLong();
    QJsonObject stat = dataObj.value("stat").toObject();
    r.playCount = stat.value("view").toVariant().toLongLong();
    r.isAvailable = true;

    return r;
}
