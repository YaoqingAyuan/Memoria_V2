#ifndef HTTPPROXYSERVER_H
#define HTTPPROXYSERVER_H
//本地HTTP代理服务器：为QMediaPlayer提供带Referer/Cookie头的B站CDN访问
//方案B：通过本地代理注入HTTP头，绕过B站CDN防盗链(方案C的URL参数注入因QUrl百分号编码失效)

#include <QObject>
#include <QTcpServer>
#include <QNetworkAccessManager>

class HttpProxyServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpProxyServer(QObject *parent = nullptr);

    bool start();  //在127.0.0.1上监听，端口自动分配
    quint16 port() const { return m_port; }

    void setReferer(const QString &r) { m_referer = r; }
    void setCookie(const QString &c) { m_cookie = c; }

    //为给定CDN URL构造代理URL: http://127.0.0.1:port/proxy?url=ENCODED_TARGET
    QUrl proxyUrl(const QUrl &targetUrl) const;

private slots:
    void onNewConnection();

private:
    QTcpServer *m_server;
    QNetworkAccessManager *m_nam;
    quint16 m_port = 0;
    QString m_referer = "https://www.bilibili.com";
    QString m_cookie;
};

#endif // HTTPPROXYSERVER_H
