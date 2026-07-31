#include "WLAN_Input_Weight.h"
#include "ui_WLAN_Input_Weight.h"

WLAN_Input_Weight::WLAN_Input_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WLAN_Input_Weight)
{
    ui->setupUi(this);
}

WLAN_Input_Weight::~WLAN_Input_Weight()
{
    delete ui;
}
