#include "Link_Input_Weight.h"
#include "ui_Link_Input_Weight.h"

Link_Input_Weight::Link_Input_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Link_Input_Weight)
{
    ui->setupUi(this);
}

Link_Input_Weight::~Link_Input_Weight()
{
    delete ui;
}
