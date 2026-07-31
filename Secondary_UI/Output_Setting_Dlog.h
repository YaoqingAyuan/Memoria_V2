#ifndef OUTPUT_SETTING_DLOG_H
#define OUTPUT_SETTING_DLOG_H
//二级UI：输出设置UI，搭建格式选择与参数设置(.webm)组件

#include <QDialog>

namespace Ui {
class Output_Setting_Dlog;
}

class Output_Setting_Dlog : public QDialog
{
    Q_OBJECT

public:
    explicit Output_Setting_Dlog(QWidget *parent = nullptr);
    ~Output_Setting_Dlog();

private:
    Ui::Output_Setting_Dlog *ui;
};

#endif // OUTPUT_SETTING_DLOG_H
