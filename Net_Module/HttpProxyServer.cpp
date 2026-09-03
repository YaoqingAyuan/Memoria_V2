#include "HttpProxyServer.h"
#include <QTcpSocket>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include "Core/logger.h"

HttpProxyServer::HttpProxyServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_nam(new QNetworkAccessManager(this))
{
}

bool HttpProxyServer::start()
{
    if (m_server->isListening())
        return true;
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        Logger::instance()->warning("Proxy", "代理服务器启动失败");
        return false;
    }
    m_port = m_server->serverPort();
    connect(m_server, &QTcpServer::newConnection, this, &HttpProxyServer::onNewConnection);
    Logger::instance()->debug("Proxy", QString("HTTP代理已启动, 端口: %1").arg(m_port));
    return true;
}

QUrl HttpProxyServer::proxyUrl(const QUrl &targetUrl) const
{
    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(static_cast<int>(m_port));
    url.setPath("/proxy");
    QUrlQuery query;
    query.addQueryItem("url", targetUrl.toString());
    url.setQuery(query);
    return url;
}

void HttpProxyServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *client = m_server->nextPendingConnection();
        client->setProperty("processed", false);

        connect(client, &QTcpSocket::readyRead, this, [this, client]() {
            if (client->property("processed").toBool())
                return;

            QByteArray data = client->readAll();
            QString request = QString::fromUtf8(data);

            //等待完整HTTP请求头(以\r\n\r\n结尾)
            if (!request.contains("\r\n\r\n"))
                return;

            client->setProperty("processed", true);

            //解析请求行: GET /proxy?url=... HTTP/1.1
            int firstSpace = request.indexOf(' ');
            int secondSpace = request.indexOf(' ', firstSpace + 1);
            if (firstSpace < 0 || secondSpace < 0) {
                client->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                client->disconnectFromHost();
                return;
            }

            QString fullPath = request.mid(firstSpace + 1, secondSpace - firstSpace - 1);

            //从查询参数提取目标URL
            int queryStart = fullPath.indexOf('?');
            if (queryStart < 0) {
                client->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                client->disconnectFromHost();
                return;
            }

            QUrlQuery query(fullPath.mid(queryStart + 1));
            QString targetUrlStr = query.queryItemValue("url", QUrl::FullyDecoded);
            QUrl targetUrl(targetUrlStr);
            if (!targetUrl.isValid()) {
                Logger::instance()->warning("Proxy", QString("无效的目标URL: %1").arg(targetUrlStr));
                client->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                client->disconnectFromHost();
                return;
            }

            //解析Range头(FFmpeg seek时发送)
            QString rangeHeader;
            int rangeIdx = request.indexOf("Range:", 0, Qt::CaseInsensitive);
            if (rangeIdx >= 0) {
                int rangeEnd = request.indexOf("\r\n", rangeIdx);
                rangeHeader = request.mid(rangeIdx + 6, rangeEnd - rangeIdx - 6).trimmed();
            }

            //构造转发请求
            QNetworkRequest req(targetUrl);
            req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
            req.setRawHeader("Referer", m_referer.toUtf8());
            if (!m_cookie.isEmpty())
                req.setRawHeader("Cookie", m_cookie.toUtf8());
            if (!rangeHeader.isEmpty())
                req.setRawHeader("Range", rangeHeader.toUtf8());

            QNetworkReply *reply = m_nam->get(req);
            bool *headerSent = new bool(false);

            //收到响应数据时转发给客户端
            connect(reply, &QNetworkReply::readyRead, this, [client, reply, headerSent]() {
                if (!*headerSent) {
                    int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    QByteArray response;
                    response.append("HTTP/1.1 " + QByteArray::number(code) + " OK\r\n");

                    //转发关键响应头
                    for (const QByteArray &h : reply->rawHeaderList()) {
                        QByteArray lower = h.toLower();
                        if (lower == "content-type" || lower == "content-length" ||
                            lower == "content-range" || lower == "accept-ranges") {
                            response.append(h + ": " + reply->rawHeader(h) + "\r\n");
                        }
                    }
                    response.append("Connection: close\r\n\r\n");
                    client->write(response);
                    *headerSent = true;
                }
                client->write(reply->readAll());
            });

            connect(reply, &QNetworkReply::finished, this, [client, reply, headerSent]() {
                if (!*headerSent) {
                    client->write("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n");
                }
                client->disconnectFromHost();
                reply->deleteLater();
                delete headerSent;
            });

            connect(reply, &QNetworkReply::errorOccurred, this, [client, reply, headerSent](QNetworkReply::NetworkError) {
                if (!*headerSent) {
                    client->write("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n");
                    *headerSent = true;
                }
                client->disconnectFromHost();
                reply->deleteLater();
                delete headerSent;
            });

            connect(client, &QTcpSocket::disconnected, this, [client]() {
                client->deleteLater();
            });
        });
    }
}
