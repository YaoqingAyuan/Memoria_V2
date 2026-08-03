#include "Setting_Dialog.h"
#include "ui_Setting_Dialog.h"

Setting_Dialog::Setting_Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Setting_Dialog)
{
    ui->setupUi(this);
}

Setting_Dialog::~Setting_Dialog()
{
    delete ui;
}
