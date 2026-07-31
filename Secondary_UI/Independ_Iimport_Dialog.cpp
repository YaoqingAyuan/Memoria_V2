#include "Independ_Import_Dialog.h"
#include "ui_Independ_Import_Dialog.h"

Independ_Import_Dialog::Independ_Import_Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Independ_Import_Dialog)
{
    ui->setupUi(this);
}

Independ_Import_Dialog::~Independ_Import_Dialog()
{
    delete ui;
}
