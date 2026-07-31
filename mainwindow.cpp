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


void MainWindow::on_OutputBtn_clicked()
{

}


void MainWindow::on_OutputPath_Btn_clicked()
{

}


void MainWindow::on_VedioPath_Btn_clicked()
{

}


void MainWindow::on_AudioPath_Btn_clicked()
{

}


void MainWindow::on_PlusLine_Btn_clicked()
{

}


void MainWindow::on_DeleteLine_Btn_clicked()
{

}


void MainWindow::on_Setting_Btn_clicked()
{

}

