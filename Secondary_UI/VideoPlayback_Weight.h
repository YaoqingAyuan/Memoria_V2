#ifndef VIDEOPLAYBACK_WEIGHT_H
#define VIDEOPLAYBACK_WEIGHT_H
//预览播放按钮：搭建双屏播放界面、提供网页检索功能(下架检测)

#include <QWidget>

namespace Ui {
class VideoPlayback_Weight;
}

class VideoPlayback_Weight : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayback_Weight(QWidget *parent = nullptr);
    ~VideoPlayback_Weight();

private:
    Ui::VideoPlayback_Weight *ui;
};

#endif // VIDEOPLAYBACK_WEIGHT_H
