#ifndef OUTPUT_SETTING_DLOG_H
#define OUTPUT_SETTING_DLOG_H
//二级UI：输出设置UI，搭建格式选择与参数设置(.webm)组件

#include <QDialog>

namespace Ui {
class Output_Setting_Dlog;
}

class Output_Setting_Dlog : public QDialog
{
    Q_OBJECT

public:
    explicit Output_Setting_Dlog(QWidget *parent = nullptr);
    ~Output_Setting_Dlog();

private slots:
    // 视频参数：质量控制模式切换 → CRF / 目标码率互斥启用
    void on_VideoQualityMode_cmbBox_currentIndexChanged(int index);
    // 视频参数：无损勾选 → 禁用质量控制模式及其子组件
    void on_Lossless_chkBox_toggled(bool checked);
    // 视频参数：CRF 滑块值 → 更新数值标签
    void on_CRF_slider_valueChanged(int value);
    // 视频参数：cpu-used 滑块值 → 更新数值标签
    void on_CpuUsed_slider_valueChanged(int value);

    // 音频参数：编码器切换 → libopus 禁用比特率模式，libvorbis 启用
    void on_AudioCodec_cmbBox_currentIndexChanged(int index);
    // 音频参数：比特率模式切换 → VBR 显示质量滑块，CBR 显示码率下拉
    void on_AudioBitrateMode_cmbBox_currentIndexChanged(int index);
    // 音频参数：质量滑块值 → 更新数值标签
    void on_AudioQuality_slider_valueChanged(int value);

private:
    Ui::Output_Setting_Dlog *ui;

    // 初始化视频参数区的条件状态
    void initVideoParamsState();
    // 初始化音频参数区的条件状态
    void initAudioParamsState();
    // 更新音频质量/码率 StackWidget 的显示页面
    void updateAudioStackPage();
};

#endif // OUTPUT_SETTING_DLOG_H
