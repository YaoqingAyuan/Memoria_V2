#include "WebSearchSidebar.h"
#include "ui_WebSearchSidebar.h"
#include "Net_Module/BiliApiWorker.h"
#include "Core/logger.h"
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QMenu>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSettings>
#include <QTimer>
#include <QStyle>
#include <QLineEdit>
#include <QLabel>

WebSearchSidebar::WebSearchSidebar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WebSearchSidebar)
{
    ui->setupUi(this);

    //登录按钮支持右键菜单
    ui->loginBtn->setContextMenuPolicy(Qt::CustomContextMenu);

    //=== 信号槽 ===
    connect(ui->searchBtn, &QPushButton::clicked, this, &WebSearchSidebar::onSearchClicked);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &WebSearchSidebar::onSearchClicked);
    connect(ui->resultList, &QListWidget::itemClicked, this, &WebSearchSidebar::onResultItemClicked);
    connect(ui->loginBtn, &QPushButton::clicked, this, &WebSearchSidebar::onLoginBtnClicked);
    connect(ui->loginBtn, &QWidget::customContextMenuRequested, this, &WebSearchSidebar::onShowContextMenu);

    updateLoginUI();
}

WebSearchSidebar::~WebSearchSidebar()
{
    delete ui;
}

void WebSearchSidebar::setApiWorker(BiliApiWorker *worker)
{
    m_apiWorker = worker;

    //BiliApiWorker信号
    connect(m_apiWorker, &BiliApiWorker::searchResultReady, this, &WebSearchSidebar::onSearchResultReady);
    connect(m_apiWorker, &BiliApiWorker::searchFailed, this, &WebSearchSidebar::onSearchFailed);
    connect(m_apiWorker, &BiliApiWorker::availabilityChecked, this, &WebSearchSidebar::onAvailabilityChecked);
    connect(m_apiWorker, &BiliApiWorker::loginStatusChanged, this, &WebSearchSidebar::onLoginStatusChanged);

    updateLoginUI();
}

QString WebSearchSidebar::searchText() const
{
    return ui->searchEdit->text();
}

void WebSearchSidebar::setSearchText(const QString &text)
{
    ui->searchEdit->setText(text);
}

void WebSearchSidebar::onSearchClicked()
{
    if (!m_apiWorker)
        return;

    QString keyword = ui->searchEdit->text().trimmed();
    if (keyword.isEmpty())
        return;

    ui->statusLabel->setText(QStringLiteral("搜索中..."));
    ui->resultList->clear();
    m_results.clear();

    //如果是BV号格式(BV开头)，直接精确查询
    if (keyword.startsWith("BV", Qt::CaseInsensitive)) {
        m_apiWorker->searchByBvid(keyword);
    } else {
        m_apiWorker->searchByKeyword(keyword);
    }
}

void WebSearchSidebar::onSearchResultReady(const QList<BiliSearchResult> &results)
{
    m_results = results;
    ui->resultList->clear();

    if (results.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("无搜索结果"));
        return;
    }

    for (const auto &r : results) {
        QString display = QStringLiteral("%1\n%2 · %3 播放")
            .arg(r.title)
            .arg(r.ownerName)
            .arg(r.formattedPlayCount());
        QListWidgetItem *item = new QListWidgetItem(display, ui->resultList);
        if (!r.isAvailable)
            item->setForeground(Qt::red);
    }

    ui->statusLabel->setText(QStringLiteral("找到 %1 条结果").arg(results.size()));
}

void WebSearchSidebar::onSearchFailed(const QString &error)
{
    ui->statusLabel->setText(QStringLiteral("搜索失败: %1").arg(error));
    Logger::instance()->warning("Sidebar", QString("搜索失败: %1").arg(error));
}

void WebSearchSidebar::onAvailabilityChecked(const QString &bvid, bool isAvailable, const QString &description)
{
    QString status = isAvailable
        ? QStringLiteral("✅ %1: %2").arg(bvid, description)
        : QStringLiteral("❌ %1: %2").arg(bvid, description);
    ui->statusLabel->setText(status);
    emit availabilityUpdated(isAvailable, description);
}

void WebSearchSidebar::onResultItemClicked(QListWidgetItem *item)
{
    int row = ui->resultList->row(item);
    if (row >= 0 && row < m_results.size()) {
        emit resultSelected(m_results[row]);
    }
}

void WebSearchSidebar::onLoginBtnClicked()
{
    if (!m_apiWorker || m_apiWorker->isLoggedIn())
        return;  //已登录，点击无操作(右键才有退出)

    //请求二维码
    m_apiWorker->requestLoginQrCode();
    //二维码图片URL到达后通过qrCodeReady信号处理
    connect(m_apiWorker, &BiliApiWorker::qrCodeReady, this, [this](const QString &qrImageUrl, const QString &qrcodeKey) {
        m_qrcodeKey = qrcodeKey;
        showLoginDialog(qrImageUrl);
    }, Qt::SingleShotConnection);
}

void WebSearchSidebar::showLoginDialog(const QString &qrImageUrl)
{
    m_loginDialog = new QDialog(this);
    m_loginDialog->setWindowTitle(QStringLiteral("登录B站 — 扫码登录"));
    m_loginDialog->setFixedSize(300, 380);

    QVBoxLayout *layout = new QVBoxLayout(m_loginDialog);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(QStringLiteral("请使用B站APP扫码登录"), m_loginDialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(titleLabel);

    m_qrCodeLabel = new QLabel(m_loginDialog);
    m_qrCodeLabel->setAlignment(Qt::AlignCenter);
    m_qrCodeLabel->setMinimumSize(240, 240);
    m_qrCodeLabel->setStyleSheet("QLabel { background: white; border: 1px solid #ddd; }");
    layout->addWidget(m_qrCodeLabel);

    m_hintLabel = new QLabel(QStringLiteral("等待扫码..."), m_loginDialog);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(m_hintLabel);

    QCheckBox *autoLoginCheck = new QCheckBox(QStringLiteral("自动登录(以后打开软件时保持登录)"), m_loginDialog);
    autoLoginCheck->setChecked(true);
    layout->addWidget(autoLoginCheck);

    //下载二维码图片
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    QNetworkRequest request((QUrl(qrImageUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"));
    QNetworkReply *reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_hintLabel->setText(QStringLiteral("二维码加载失败"));
            return;
        }
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull()) {
            m_qrCodeLabel->setPixmap(pixmap.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });

    //启动轮询定时器
    m_loginPollTimer = new QTimer(this);
    connect(m_loginPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_qrcodeKey.isEmpty())
            m_apiWorker->pollLoginStatus(m_qrcodeKey);
    });
    m_loginPollTimer->start(2000);  //每2秒轮询

    m_loginDialog->exec();

    //对话框关闭后清理
    if (m_loginPollTimer) {
        m_loginPollTimer->stop();
        m_loginPollTimer->deleteLater();
        m_loginPollTimer = nullptr;
    }
    delete m_loginDialog;
    m_loginDialog = nullptr;
}

void WebSearchSidebar::closeLoginDialog()
{
    if (m_loginPollTimer) {
        m_loginPollTimer->stop();
        m_loginPollTimer->deleteLater();
        m_loginPollTimer = nullptr;
    }
    if (m_loginDialog) {
        m_loginDialog->accept();
    }
}

void WebSearchSidebar::onLoginStatusChanged(int code, const QString &message, const QString &cookie)
{
    if (!m_loginDialog)
        return;

    switch (code) {
    case 0:  //登录成功
        if (m_loginPollTimer)
            m_loginPollTimer->stop();
        m_loginDialog->accept();
        updateLoginUI();
        emit loginStateChanged(true);
        Logger::instance()->debug("Sidebar", "B站登录成功");
        break;
    case 86090:  //已扫码等待确认
        if (m_hintLabel)
            m_hintLabel->setText(QStringLiteral("已扫码，请在手机上确认"));
        break;
    case 86101:  //未扫码
        //保持"等待扫码..."
        break;
    case 86038:  //二维码失效
        if (m_loginPollTimer)
            m_loginPollTimer->stop();
        if (m_hintLabel) {
            m_hintLabel->setText(QStringLiteral("二维码已失效，请关闭后重试"));
            m_hintLabel->setStyleSheet("color: red; font-size: 12px;");
        }
        break;
    default:
        break;
    }
}

void WebSearchSidebar::onShowContextMenu(const QPoint &pos)
{
    if (!m_apiWorker || !m_apiWorker->isLoggedIn())
        return;

    QMenu menu(this);
    QAction *logoutAction = menu.addAction(QStringLiteral("退出登录"));

    QAction *selected = menu.exec(ui->loginBtn->mapToGlobal(pos));
    if (selected == logoutAction) {
        m_apiWorker->clearCookie();
        updateLoginUI();
        emit loginStateChanged(false);
        Logger::instance()->debug("Sidebar", "已退出B站登录");
    }
}

void WebSearchSidebar::updateLoginUI()
{
    if (!m_apiWorker || !m_apiWorker->isLoggedIn()) {
        ui->loginBtn->setText(QStringLiteral("🔵 登录B站"));
        ui->loginBtn->setStyleSheet("QPushButton { text-align: left; padding: 4px 8px; border: none; color: #888; }");
        ui->loginStatusLabel->setText(QStringLiteral("登录后可获取高清流"));
    } else {
        ui->loginBtn->setText(QStringLiteral("🔵 已登录B站"));
        ui->loginBtn->setStyleSheet("QPushButton { text-align: left; padding: 4px 8px; border: none; color: #00a1d6; }");
        ui->loginStatusLabel->setText(QStringLiteral("右键可退出登录"));
    }
}
