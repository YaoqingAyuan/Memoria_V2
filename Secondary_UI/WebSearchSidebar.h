#ifndef WEBSEARCHSIDEBAR_H
#define WEBSEARCHSIDEBAR_H
//网页检索(WebSearch)侧边栏(Sidebar)
//包含：搜索栏、搜索结果列表、相关视频区域、登录B站入口(二维码对话框)
//右键"登录B站"选项弹出退出登录菜单
//UI组件在 .ui 文件中设计，此处仅处理业务逻辑

#include <QWidget>
#include <QList>
#include "Net_Module/BiliSearchResult.h"

class BiliApiWorker;
class QMenu;
class QDialog;
class QLabel;
class QTimer;
class QListWidgetItem;

namespace Ui {
class WebSearchSidebar;
}

class WebSearchSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit WebSearchSidebar(QWidget *parent = nullptr);
    ~WebSearchSidebar();

    //设置B站API工(Work)er(由VideoPlayback_Weight在构造后调用)
    void setApiWorker(BiliApiWorker *worker);

    //获取搜索框文本(供外部预填)
    QString searchText() const;
    void setSearchText(const QString &text);

signals:
    //用户选中某个搜索结果(供VideoPlayback_Weight加载在线播放)
    void resultSelected(const BiliSearchResult &result);
    //下架检测结果更新
    void availabilityUpdated(bool isAvailable, const QString &description);
    //登录状态变化
    void loginStateChanged(bool loggedIn);

private slots:
    void onSearchClicked();
    void onSearchResultReady(const QList<BiliSearchResult> &results);
    void onSearchFailed(const QString &error);
    void onAvailabilityChecked(const QString &bvid, bool isAvailable, const QString &description);
    void onResultItemClicked(QListWidgetItem *item);
    void onLoginBtnClicked();
    void onLoginStatusChanged(int code, const QString &message, const QString &cookie);
    void onShowContextMenu(const QPoint &pos);

private:
    Ui::WebSearchSidebar *ui;
    BiliApiWorker *m_apiWorker = nullptr;

    //搜索结果缓存(索引对应列表项)
    QList<BiliSearchResult> m_results;

    //登录对话框
    QDialog *m_loginDialog = nullptr;
    QLabel *m_qrCodeLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QString m_qrcodeKey;
    QTimer *m_loginPollTimer = nullptr;

    //更新登录按钮显示
    void updateLoginUI();
    //显示二维码登录对话框
    void showLoginDialog(const QString &qrImageUrl);
    //关闭登录对话框
    void closeLoginDialog();
};

#endif // WEBSEARCHSIDEBAR_H
