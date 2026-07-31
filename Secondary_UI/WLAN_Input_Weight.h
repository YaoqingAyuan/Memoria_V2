#ifndef WLAN_INPUT_WEIGHT_H
#define WLAN_INPUT_WEIGHT_H
//Wifi无线链接导入窗口：搭建相关组件与UI

#include <QWidget>

namespace Ui {
class WLAN_Input_Weight;
}

class WLAN_Input_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit WLAN_Input_Weight(QWidget *parent = nullptr);
    ~WLAN_Input_Weight();

private:
    Ui::WLAN_Input_Weight *ui;
};

#endif // WLAN_INPUT_WEIGHT_H
