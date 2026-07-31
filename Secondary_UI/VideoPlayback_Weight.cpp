#include "VideoPlayback_Weight.h"
#include "ui_VideoPlayback_Weight.h"

VideoPlayback_Weight::VideoPlayback_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPlayback_Weight)
{
    ui->setupUi(this);
}

VideoPlayback_Weight::~VideoPlayback_Weight()
{
    delete ui;
}
