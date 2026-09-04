#ifndef BILIAPIWORKER_H
#define BILIAPIWORKER_H
//B站(Bili)API请求工(Work)er：封装所有B站网络请求
//职责：关键词搜索、BV号查询、下架检测、获取播放地址、二维码登录
//所有请求异步执行，通过信号返回结果

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include "Net_Module/BiliSearchResult.h"

class BiliApiWorker : public QObject
{
    Q_OBJECT
public:
    explicit BiliApiWorker(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~BiliApiWorker();

    //=== 搜索 ===
    //按关键词搜索视频
    void searchByKeyword(const QString &keyword, int page = 1);
    //按BV/AV号精确查询视频信息(同时用于下架检测)
    //isBvid=true用bvid参数查询，isBvid=false用aid参数查询
    void searchById(const QString &id, bool isBvid);

    //=== 下架检测 ===
    //检查指定BV号视频是否可访问(返回isAvailable + 详情)
    void checkAvailability(const QString &bvid);

    //=== 获取播放地址 ===
    //获取MP4格式视频流地址(供未登录即时播放，通过代理注入Referer)
    //quality: 16=360p, 32=480p, 64=720p, 80=1080p
    void fetchPlayUrl(const QString &bvid, int cid, int quality = 32);

    //获取DASH格式视频流地址(供已登录高清播放，FFmpeg混流后播放)
    //quality: 80=1080P, 112=1080P+, 116=1080P60, 120=4K, 125=HDR, 127=8K
    //fnval=4048: DASH+HDR+4K+Dolby+8K+AV1，API返回用户可访问的最高画质
    void fetchPlayUrlDash(const QString &bvid, int cid, int quality = 127);

    //=== 二维码登录 ===
    //申请登录二维码(返回二维码图片URL + qrcode_key)
    void requestLoginQrCode();
    //轮询登录状态(qrcode_key从requestLoginQrCode获得)
    void pollLoginStatus(const QString &qrcodeKey);

    //=== Cookie管理 ===
    //设置Cookie(登录后传入SESSDATA等)
    void setCookie(const QString &cookie);
    //获取当前Cookie
    QString cookie() const { return m_cookie; }
    //清除Cookie(退出登录)
    void clearCookie();
    //是否已登录
    bool isLoggedIn() const { return !m_cookie.isEmpty(); }

    //从QSettings加载/保存Cookie
    void loadCookie();
    void saveCookie();

signals:
    //搜索结果
    void searchResultReady(const QList<BiliSearchResult> &results);
    //搜索失败
    void searchFailed(const QString &error);

    //下架检测结果(bvid, 是否在架, 原因描述)
    void availabilityChecked(const QString &bvid, bool isAvailable, const QString &description);

    //播放地址获取结果(MP4格式，可直接用QMediaPlayer通过代理播放)
    void playUrlReady(const QString &bvid, const QString &playUrl);
    //DASH格式播放地址(分离的音视频URL，需FFmpeg混流后播放)
    void playUrlDashReady(const QString &bvid, const QString &videoUrl, const QString &audioUrl);
    void playUrlFailed(const QString &error);

    //二维码登录
    void qrCodeReady(const QString &qrImageUrl, const QString &qrcodeKey);
    void loginStatusChanged(int code, const QString &message, const QString &cookie);
    //code: 0=成功, 86090=二维码已扫码未确认, 86101=未扫码, 86090=已扫码等待确认, 86038=二维码失效

private:
    QNetworkAccessManager *m_nam;
    QString m_cookie;    //Cookie字符串(SESSDATA=xxx; bili_jct=xxx)

    //创建带通用Header的请求
    QNetworkRequest createRequest(const QUrl &url);
    //解析搜索API返回的JSON
    QList<BiliSearchResult> parseSearchResults(const QByteArray &data);
    //解析视频信息API返回的JSON(view接口)
    BiliSearchResult parseVideoInfo(const QByteArray &data);

    //获取播放地址内部实现(MP4/DASH通用)
    void fetchPlayUrlInternal(const QString &bvid, int cid, int quality, bool dash);
};

#endif // BILIAPIWORKER_H
