#include "LoginManager.h"
#include "Net_Module/BiliApiWorker.h"
#include "core/logger.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include "ThirdParty/qrcodegen.hpp"

LoginManager::LoginManager(BiliApiWorker *apiWorker, QWidget *parentWidget)
    : QObject(parentWidget)
    , m_apiWorker(apiWorker)
    , m_parentWidget(parentWidget)
{
    connect(m_apiWorker, &BiliApiWorker::loginStatusChanged, this, &LoginManager::onLoginStatusChanged);
}

LoginManager::~LoginManager()
{
    closeLoginDialog();
}

void LoginManager::startLogin()
{
    if (!m_apiWorker || m_apiWorker->isLoggedIn())
        return;

    m_apiWorker->requestLoginQrCode();
    connect(m_apiWorker, &BiliApiWorker::qrCodeReady, this, [this](const QString &qrImageUrl, const QString &qrcodeKey) {
        m_qrcodeKey = qrcodeKey;
        showLoginDialog(qrImageUrl);
    }, Qt::SingleShotConnection);
}

void LoginManager::showLoginDialog(const QString &qrImageUrl)
{
    m_loginDialog = new QDialog(m_parentWidget);
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

    m_autoLoginCheck = new QCheckBox(QStringLiteral("自动登录(以后打开软件时保持登录)"), m_loginDialog);
    m_autoLoginCheck->setChecked(true);
    layout->addWidget(m_autoLoginCheck);

    //用Nayuki QR库将url字符串本地渲染为二维码(不依赖网络下载图片)
    try {
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
            qrImageUrl.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        int qrSize = qr.getSize();
        int scale = 8;  //每个模块8像素
        int margin = 4 * scale;  //4模块留白
        int imgSize = (qrSize + 8) * scale;
        QImage image(imgSize, imgSize, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                if (qr.getModule(x, y)) {
                    for (int dy = 0; dy < scale; dy++)
                        for (int dx = 0; dx < scale; dx++)
                            image.setPixel(margin + x * scale + dx, margin + y * scale + dy, 0xFF000000);
                }
            }
        }
        if (m_qrCodeLabel) {
            m_qrCodeLabel->setPixmap(QPixmap::fromImage(image).scaled(
                220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } catch (const std::exception &e) {
        if (m_hintLabel)
            m_hintLabel->setText(QStringLiteral("二维码生成失败"));
    }

    //启动轮询定时器
    m_loginPollTimer = new QTimer(this);
    connect(m_loginPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_qrcodeKey.isEmpty())
            m_apiWorker->pollLoginStatus(m_qrcodeKey);
    });
    m_loginPollTimer->start(2000);

    m_loginDialog->exec();

    if (m_loginPollTimer) {
        m_loginPollTimer->stop();
        m_loginPollTimer->deleteLater();
        m_loginPollTimer = nullptr;
    }
    delete m_loginDialog;
    m_loginDialog = nullptr;
    m_autoLoginCheck = nullptr;
}

void LoginManager::closeLoginDialog()
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

void LoginManager::onLoginStatusChanged(int code, const QString &message, const QString &cookie)
{
    Q_UNUSED(message)
    if (!m_loginDialog)
        return;

    switch (code) {
    case 0:  //登录成功
        if (m_loginPollTimer)
            m_loginPollTimer->stop();
        //读取"自动登录"复选框状态
        if (m_autoLoginCheck)
            m_rememberLogin = m_autoLoginCheck->isChecked();
        //未勾选"自动登录"时，从QSettings删除Cookie(仅当前会话有效)
        if (!m_rememberLogin) {
            QSettings settings;
            settings.remove("bili/cookie");
        }
        m_loginDialog->accept();
        emit loginChanged();
        Logger::instance()->debug("LoginManager", "B站登录成功");
        break;
    case 86090:  //已扫码等待确认
        if (m_hintLabel)
            m_hintLabel->setText(QStringLiteral("已扫码，请在手机上确认"));
        break;
    case 86101:  //未扫码
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

void LoginManager::showContextMenu(const QPoint &pos, QWidget *mapToWidget)
{
    if (!m_apiWorker || !m_apiWorker->isLoggedIn())
        return;

    QMenu menu(m_parentWidget);
    QAction *logoutAction = menu.addAction(QStringLiteral("退出登录"));

    QAction *selected = menu.exec(mapToWidget->mapToGlobal(pos));
    if (selected == logoutAction) {
        m_apiWorker->clearCookie();
        emit loginChanged();
        Logger::instance()->debug("LoginManager", "已退出B站登录");
    }
}
