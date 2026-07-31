#include "Output_Setting_Dlog.h"
#include "ui_Output_Setting_Dlog.h"

Output_Setting_Dlog::Output_Setting_Dlog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Output_Setting_Dlog)
{
    ui->setupUi(this);
}

Output_Setting_Dlog::~Output_Setting_Dlog()
{
    delete ui;
}
