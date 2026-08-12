#ifndef SETTING_DIALOG_H
#define SETTING_DIALOG_H
//设置(Setting)对话框(Dialog)：管理应用配置
//Tab1：列头设置(可选列的显隐控制)
//Tab2：缓存管理(关闭时清理策略、过期天数、手动清理)

#include <QDialog>
#include <QSettings>

namespace Ui {
class Setting_Dialog;
}

class DataModel;

class Setting_Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Setting_Dialog(DataModel *model, QWidget *parent = nullptr);
    ~Setting_Dialog();

private slots:
    //=== Tab1：列头设置 ===
    void on_applyBtn_clicked();
    void on_closeBtn_clicked();

    //=== Tab2：缓存管理 ===
    //关闭时清理CheckBox状态变化 → 启用/禁用过期天数下拉
    void onCleanOnCloseChanged(int state);
    //清理全部缓存按钮 → 调用CacheManager::cleanAll并刷新大小显示
    void onCleanCacheNowClicked();
    //过期天数下拉变化 → 保存到QSettings
    void onExpiryDaysChanged(int index);

private:
    Ui::Setting_Dialog *ui;
    DataModel *m_model;     //关联的表格数据模型(不持有所有权)

    //Tab1：从DataModel/配置同步复选框
    void syncCheckboxesFromModel();
    void syncCheckboxesFromSettings();

    //Tab2：加载缓存设置到UI
    void loadCacheSettings();
    //Tab2：刷新缓存大小显示
    void refreshCacheSize();
};

#endif // SETTING_DIALOG_H
