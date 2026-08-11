#ifndef WLAN_INPUT_WEIGHT_H
#define WLAN_INPUT_WEIGHT_H
//Wifi无线链接导入窗口：搭建相关组件与UI

#include <QWidget>

namespace Ui {
class WLAN_Input_Weight;
}

class QTreeWidgetItem;

class WLAN_Input_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit WLAN_Input_Weight(QWidget *parent = nullptr);
    ~WLAN_Input_Weight();

signals:
    //确认导入：通知MainWindow将解析好的数据写入DataModel表格
    void importConfirmed();

private slots:
    void onRefreshDevices();
    void onDeviceContextMenu(const QPoint &pos);
    void onFileItemChanged(QTreeWidgetItem *item, int column);
    void onParseSelected();
    void onConfirmImport();
    void onSelectAllChanged(bool checked);
    void onPreviewItemChanged(QTreeWidgetItem *item, int column);
    void onWifiSelectionChanged(int index);
    void onConnectWifi();
    void onUpButtonClicked();

private:
    void initUI();
    void populateDemoData();

    Ui::WLAN_Input_Weight *ui;
};

#endif // WLAN_INPUT_WEIGHT_H
