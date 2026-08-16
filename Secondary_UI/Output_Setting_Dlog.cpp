#include "Output_Setting_Dlog.h"
#include "ui_Output_Setting_Dlog.h"
#include <QMessageBox>

Output_Setting_Dlog::Output_Setting_Dlog(const QList<int> &selectedRows, int totalRowCount,
                                         QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Output_Setting_Dlog)
    , m_selectedRows(selectedRows)
    , m_totalRowCount(totalRowCount)
{
    ui->setupUi(this);

    // 锁死窗口大小（无法拖拽改变长宽）
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);

    // 初始化各参数区的条件状态
    initVideoParamsState();
    initAudioParamsState();

    //根据默认格式(.mp4=复制流)设置参数区启用状态
    updateFormatDependentState();
}

Output_Setting_Dlog::~Output_Setting_Dlog()
{
    delete ui;
}

// =========================================================================
// 格式选择
// =========================================================================

//格式切换：仅.webm启用音视频参数区，其余格式禁用(复制流无需参数)
void Output_Setting_Dlog::on_FormChoose_cmbBox_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    updateFormatDependentState();
}

//根据当前格式启用/禁用音视频参数区
//FormChoose_cmbBox: 0=.mp4 1=.mkv 2=.mov 3=.webm
//仅.webm(转码)需要参数；.mp4/.mkv/.mov(复制流)无需参数，整组灰显
void Output_Setting_Dlog::updateFormatDependentState()
{
    bool isWebm = (ui->FormChoose_cmbBox->currentIndex() == 3);
    ui->VideoParams_Group->setEnabled(isWebm);
    ui->AudioParams_Group->setEnabled(isWebm);
}

// =========================================================================
// 确定按钮：校验 + 收集导出配置
// =========================================================================

void Output_Setting_Dlog::accept()
{
    //根据单选按钮确定待导出行
    if (ui->OutputFirst_Btn->isChecked()) {
        //"导出首项"：仅序号为1的行(索引0)
        if (m_totalRowCount == 0) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("表格中没有数据行，无法导出"));
            return;
        }
        m_targetRows = {0};
    } else if (ui->OutputSelect_Btn->isChecked()) {
        //"导出选中项"：仅先前选中的行
        if (m_selectedRows.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("没有选中任何行，无法导出"));
            return;
        }
        m_targetRows = m_selectedRows;
    } else if (ui->OutputAll_Btn->isChecked()) {
        //"导出全部"：表格中所有行
        if (m_totalRowCount == 0) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("表格中没有数据行，无法导出"));
            return;
        }
        m_targetRows.clear();
        for (int i = 0; i < m_totalRowCount; ++i)
            m_targetRows.append(i);
    } else {
        //未选择任何单选项(理论上不会出现)
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请选择导出范围"));
        return;
    }

    //收集输出格式
    switch (ui->FormChoose_cmbBox->currentIndex()) {
    case 0:  m_format = OutputFormat::MP4;  break;
    case 1:  m_format = OutputFormat::MKV;  break;
    case 2:  m_format = OutputFormat::MOV;  break;
    case 3:  m_format = OutputFormat::WEBM; break;
    default: m_format = OutputFormat::MP4;  break;
    }

    //收集转码参数(仅WEBM有效，其余格式复制流不使用参数)
    if (m_format == OutputFormat::WEBM) {
        //=== 视频参数 ===
        m_params.videoCodec = (ui->VideoCodec_cmbBox->currentIndex() == 0)
                              ? QStringLiteral("libvpx-vp9") : QStringLiteral("libvpx");
        m_params.lossless = ui->Lossless_chkBox->isChecked();
        m_params.useCrf = (ui->VideoQualityMode_cmbBox->currentIndex() == 0);
        m_params.crf = ui->CRF_slider->value();
        //目标码率：解析数值+单位(kbps/Mbps)→统一转kbps
        int bitrateVal = ui->TargetBitrate_Edit->text().trimmed().toInt();
        if (bitrateVal <= 0) bitrateVal = 750;
        m_params.targetBitrateKbps = (ui->BitrateUnit_cmbBox->currentIndex() == 1)
                                     ? bitrateVal * 1000 : bitrateVal;
        m_params.cpuUsed = ui->CpuUsed_slider->value();
        m_params.deadline = ui->Deadline_cmbBox->currentText();

        //=== 音频参数 ===
        m_params.audioCodec = (ui->AudioCodec_cmbBox->currentIndex() == 0)
                              ? QStringLiteral("libopus") : QStringLiteral("libvorbis");
        m_params.audioVbr = (ui->AudioBitrateMode_cmbBox->currentIndex() == 0);
        m_params.audioVbrQuality = ui->AudioQuality_slider->value();
        //AudioBitrate_cmbBox文本如"128k"，提取数值部分
        QString bitrateText = ui->AudioBitrate_cmbBox->currentText();
        m_params.audioBitrate = bitrateText.remove('k').remove('K').trimmed().toInt();
        if (m_params.audioBitrate <= 0)
            m_params.audioBitrate = 128;  //兜底默认值
    }

    QDialog::accept();
}

// =========================================================================
// 数据获取(供主窗口在Accepted后读取)
// =========================================================================

QList<int> Output_Setting_Dlog::targetRowIndices() const
{
    return m_targetRows;
}

OutputFormat Output_Setting_Dlog::selectedFormat() const
{
    return m_format;
}

TranscodeParams Output_Setting_Dlog::transcodeParams() const
{
    return m_params;
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
