#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

//输出按钮(路径设置、导出)
void MainWindow::on_OutputBtn_clicked()
{

}


void MainWindow::on_OutputPath_Btn_clicked()
{

}

//加减行按钮
void MainWindow::on_PlusLine_Btn_clicked()
{

}


void MainWindow::on_DeleteLine_Btn_clicked()
{

}

//独立导入按钮
void MainWindow::on_IndepImport_Btn_clicked()
{

}

//设置按钮
void MainWindow::on_Setting_Btn_clicked()
{

}

//导入按钮组
void MainWindow::on_Link_Input_Btn_clicked()
{

}


void MainWindow::on_WLAN_Input_Btn_clicked()
{

}


void MainWindow::on_LocalCache_Btn_clicked()
{

}



