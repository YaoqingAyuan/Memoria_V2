#ifndef LINK_INPUT_WEIGHT_H
#define LINK_INPUT_WEIGHT_H
//外部有线导入窗口：搭建数据线链接外部设备的组件与UI

#include <QWidget>

namespace Ui {
class Link_Input_Weight;
}

class Link_Input_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit Link_Input_Weight(QWidget *parent = nullptr);
    ~Link_Input_Weight();

private:
    Ui::Link_Input_Weight *ui;
};

#endif // LINK_INPUT_WEIGHT_H
