#include "HttpProxyServer.h"
#include <QTcpSocket>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QPointer>
#include <QSharedPointer>
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
            // 使用 QSharedPointer 管理 headerSent，避免 errorOccurred 与 finished 同时触发导致 double-free
            auto headerSent = QSharedPointer<bool>::create(false);
            // 使用 QPointer 保护 client，避免 client 断开后 reply 仍在写入导致野指针崩溃
            QPointer<QTcpSocket> clientPtr(client);
            // 使用 QPointer 保护 reply，避免 reply 先被销毁后 client 才断开导致野指针崩溃
            QPointer<QNetworkReply> replyPtr(reply);

            // 客户端断开时中止对应的网络请求，防止 reply 继续回调
            connect(client, &QTcpSocket::disconnected, this, [replyPtr, client]() {
                if (replyPtr && replyPtr->isRunning())
                    replyPtr->abort();
                client->deleteLater();
            });

            //收到响应数据时转发给客户端
            connect(reply, &QNetworkReply::readyRead, this, [clientPtr, replyPtr, headerSent]() {
                if (!clientPtr || !replyPtr)
                    return;
                if (!*headerSent) {
                    int code = replyPtr->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    QByteArray response;
                    response.append("HTTP/1.1 " + QByteArray::number(code) + " OK\r\n");

                    //转发关键响应头
                    for (const QByteArray &h : replyPtr->rawHeaderList()) {
                        QByteArray lower = h.toLower();
                        if (lower == "content-type" || lower == "content-length" ||
                            lower == "content-range" || lower == "accept-ranges") {
                            response.append(h + ": " + replyPtr->rawHeader(h) + "\r\n");
                        }
                    }
                    response.append("Connection: close\r\n\r\n");
                    clientPtr->write(response);
                    *headerSent = true;
                }
                clientPtr->write(replyPtr->readAll());
            });

            connect(reply, &QNetworkReply::finished, this, [clientPtr, replyPtr, headerSent]() {
                if (clientPtr) {
                    if (!*headerSent) {
                        clientPtr->write("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n");
                    }
                    clientPtr->disconnectFromHost();
                }
                if (replyPtr)
                    replyPtr->deleteLater();
                // headerSent 由 QSharedPointer 自动管理，无需手动 delete
            });

            connect(reply, &QNetworkReply::errorOccurred, this, [clientPtr, replyPtr, headerSent](QNetworkReply::NetworkError) {
                if (clientPtr && replyPtr && !*headerSent) {
                    clientPtr->write("HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n");
                    *headerSent = true;
                }
                // 不在这里 deleteLater reply，由 finished 信号统一处理，避免重复 delete
            });
        });
    }
}
