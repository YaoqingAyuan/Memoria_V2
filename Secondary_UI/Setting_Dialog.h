#ifndef SETTING_DIALOG_H
#define SETTING_DIALOG_H
//设置(Setting)对话框(Dialog)：管理应用配置
//当前功能：列头设置(可选列的显隐控制)，未来扩展其他设置项

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
    //应用按钮：将复选框状态写入DataModel并保存到配置
    void on_applyBtn_clicked();
    //关闭按钮：直接关闭对话框
    void on_closeBtn_clicked();

private:
    Ui::Setting_Dialog *ui;
    DataModel *m_model;     //关联的表格数据模型(不持有所有权)

    //从DataModel读取当前列可见性，同步到复选框
    void syncCheckboxesFromModel();
    //从配置读取列可见性，同步到复选框(首次打开时使用)
    void syncCheckboxesFromSettings();
};

#endif // SETTING_DIALOG_H
