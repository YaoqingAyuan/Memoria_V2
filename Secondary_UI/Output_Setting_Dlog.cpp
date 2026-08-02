#include "Output_Setting_Dlog.h"
#include "ui_Output_Setting_Dlog.h"

Output_Setting_Dlog::Output_Setting_Dlog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Output_Setting_Dlog)
{
    ui->setupUi(this);

    // 锁死窗口大小（无法拖拽改变长宽）
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);

    // 初始化各参数区的条件状态
    initVideoParamsState();
    initAudioParamsState();
}

Output_Setting_Dlog::~Output_Setting_Dlog()
{
    delete ui;
}

// =========================================================================
// 视频参数
// =========================================================================

void Output_Setting_Dlog::initVideoParamsState()
{
    // 默认选择 CRF 模式（index 0）
    // CRF 滑块可用，目标码率禁用
    ui->VideoQualityMode_cmbBox->setCurrentIndex(0);
    ui->CRF_slider->setEnabled(true);
    ui->CRF_Value_Label->setEnabled(true);
    ui->TargetBitrate_Edit->setEnabled(false);
    ui->BitrateUnit_cmbBox->setEnabled(false);
    ui->TargetBitrate_Label->setEnabled(false);

    // 无损默认未勾选
    ui->Lossless_chkBox->setChecked(false);
}

void Output_Setting_Dlog::on_VideoQualityMode_cmbBox_currentIndexChanged(int index)
{
    // 无损模式下不响应质量控制模式的切换
    if (ui->Lossless_chkBox->isChecked())
        return;

    // index 0 = CRF, index 1 = 目标码率
    bool isCrf = (index == 0);

    ui->CRF_slider->setEnabled(isCrf);
    ui->CRF_Value_Label->setEnabled(isCrf);
    ui->CRF_Label->setEnabled(isCrf);

    ui->TargetBitrate_Edit->setEnabled(!isCrf);
    ui->BitrateUnit_cmbBox->setEnabled(!isCrf);
    ui->TargetBitrate_Label->setEnabled(!isCrf);
}

void Output_Setting_Dlog::on_Lossless_chkBox_toggled(bool checked)
{
    // 勾选无损 → 禁用质量控制模式及其全部子组件
    ui->VideoQualityMode_cmbBox->setEnabled(!checked);
    ui->VideoQualityMode_Label->setEnabled(!checked);

    ui->CRF_slider->setEnabled(!checked);
    ui->CRF_Value_Label->setEnabled(!checked);
    ui->CRF_Label->setEnabled(!checked);

    ui->TargetBitrate_Edit->setEnabled(!checked);
    ui->BitrateUnit_cmbBox->setEnabled(!checked);
    ui->TargetBitrate_Label->setEnabled(!checked);
}

void Output_Setting_Dlog::on_CRF_slider_valueChanged(int value)
{
    ui->CRF_Value_Label->setText(QString::number(value));
}

void Output_Setting_Dlog::on_CpuUsed_slider_valueChanged(int value)
{
    ui->CpuUsed_Value_Label->setText(QString::number(value));
}

// =========================================================================
// 音频参数
// =========================================================================

void Output_Setting_Dlog::initAudioParamsState()
{
    // 默认选择 libopus（index 0）
    ui->AudioCodec_cmbBox->setCurrentIndex(0);
    // libopus 时比特率模式不可选（始终用码率控制）
    ui->AudioBitrateMode_cmbBox->setEnabled(false);
    ui->AudioBitrateMode_Label->setEnabled(false);

    // 默认显示码率下拉页（page 1）
    updateAudioStackPage();
}

void Output_Setting_Dlog::on_AudioCodec_cmbBox_currentIndexChanged(int index)
{
    // index 0 = libopus, index 1 = libvorbis
    bool isVorbis = (index == 1);

    // libvorbis 才需要选择比特率模式；libopus 始终用码率
    ui->AudioBitrateMode_cmbBox->setEnabled(isVorbis);
    ui->AudioBitrateMode_Label->setEnabled(isVorbis);

    updateAudioStackPage();
}

void Output_Setting_Dlog::on_AudioBitrateMode_cmbBox_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    updateAudioStackPage();
}

void Output_Setting_Dlog::updateAudioStackPage()
{
    int codecIndex = ui->AudioCodec_cmbBox->currentIndex();
    // index 0 = libopus, index 1 = libvorbis

    if (codecIndex == 0) {
        // libopus → 始终显示码率下拉（page 1）
        ui->AudioQualityStack->setCurrentIndex(1);
    } else {
        // libvorbis → 取决于比特率模式
        int modeIndex = ui->AudioBitrateMode_cmbBox->currentIndex();
        // index 0 = VBR → 质量滑块（page 0）
        // index 1 = CBR → 码率下拉（page 1）
        ui->AudioQualityStack->setCurrentIndex(modeIndex);
    }
}

void Output_Setting_Dlog::on_AudioQuality_slider_valueChanged(int value)
{
    ui->AudioQuality_Value_Label->setText(QString::number(value));
}
