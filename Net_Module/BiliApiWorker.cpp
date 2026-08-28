#include "BiliApiWorker.h"
#include "Core/logger.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
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
            emit playUrlFailed(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        int code = root.value("code").toInt();
        if (code != 0) {
            emit playUrlFailed(root.value("message").toString());
            return;
        }

        QJsonObject data = root.value("data").toObject();
        QJsonArray durl = data.value("durl").toArray();
        if (durl.isEmpty()) {
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
            //登录成功，从响应头提取Cookie
            newCookie = reply->rawHeader("set-cookie");
            //简化处理：提取SESSDATA和bili_jct
            //完整实现需要解析set-cookie头部
            if (newCookie.isEmpty()) {
                //尝试从URL参数获取
                QString url2 = data.value("url").toString();
                //URL中包含SESSDATA等cookie信息
                QUrl cookieUrl(url2);
                QUrlQuery cookieQuery(cookieUrl.query());
                QString sessdata = cookieQuery.queryItemValue("SESSDATA");
                QString biliJct = cookieQuery.queryItemValue("bili_jct");
                if (!sessdata.isEmpty()) {
                    newCookie = QStringLiteral("SESSDATA=%1; bili_jct=%2").arg(sessdata, biliJct);
                }
            }
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
