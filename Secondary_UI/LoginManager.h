#ifndef LOGINMANAGER_H
#define LOGINMANAGER_H
//B站登录管理器：封装二维码登录对话框、轮询、自动登录逻辑
//从 VideoPlayback_Weight 中提取，UI类通过 loginChanged() 信号更新按钮状态

#include <QObject>
#include <QString>
#include <QPoint>

class BiliApiWorker;
class QWidget;
class QDialog;
class QLabel;
class QTimer;
class QCheckBox;

class LoginManager : public QObject
{
    Q_OBJECT
public:
    explicit LoginManager(BiliApiWorker *apiWorker, QWidget *parentWidget);
    ~LoginManager();

    //发起登录流程：请求二维码 → 显示对话框 → 轮询登录状态
    void startLogin();

    //显示右键菜单（退出登录）
    void showContextMenu(const QPoint &pos, QWidget *mapToWidget);

signals:
    //登录状态变化（登录成功/退出登录），UI类应据此更新按钮显示
    void loginChanged();

private:
    void showLoginDialog(const QString &qrImageUrl);
    void closeLoginDialog();
    void onLoginStatusChanged(int code, const QString &message, const QString &cookie);

    BiliApiWorker *m_apiWorker;
    QWidget *m_parentWidget;
    QDialog *m_loginDialog = nullptr;
    QLabel *m_qrCodeLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QString m_qrcodeKey;
    QTimer *m_loginPollTimer = nullptr;
    QCheckBox *m_autoLoginCheck = nullptr;  //登录对话框'自动登录'复选框
    bool m_rememberLogin = true;             //是否记住登录状态
};

#endif // LOGINMANAGER_H
