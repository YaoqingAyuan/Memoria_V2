#ifndef OUTPUT_SETTING_DLOG_H
#define OUTPUT_SETTING_DLOG_H
//二级UI：输出设置UI，搭建格式选择与参数设置(.webm)组件

#include <QDialog>
#include <QList>
#include "FFmpeg_module.h"    //OutputFormat枚举、TranscodeParams结构体

namespace Ui {
class Output_Setting_Dlog;
}

class Output_Setting_Dlog : public QDialog
{
    Q_OBJECT

public:
    //selectedRows: 主窗口表格当前选中的行索引列表(供"导出选中项"使用)
    //totalRowCount: 表格总行数(供"导出首项""导出全部"使用)
    explicit Output_Setting_Dlog(const QList<int> &selectedRows, int totalRowCount,
                                 QWidget *parent = nullptr);
    ~Output_Setting_Dlog();

    //=== 确定后供主窗口读取的导出配置 ===
    QList<int> targetRowIndices() const;        //待导出的行索引列表(基于单选按钮选择)
    OutputFormat selectedFormat() const;        //输出格式
    TranscodeParams transcodeParams() const;    //转码参数(仅WEBM格式有效)

public slots:
    void accept() override;

private slots:
    // 格式选择切换 → 仅.webm启用音视频参数组件，其余格式禁用(复制流无需参数)
    void on_FormChoose_cmbBox_currentIndexChanged(int index);

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

    //主窗口传入的上下文
    QList<int> m_selectedRows;      //当前选中的行索引
    int m_totalRowCount;            //表格总行数

    //确定后收集的导出配置(供getter返回)
    QList<int> m_targetRows;        //待导出行索引
    OutputFormat m_format;          //输出格式
    TranscodeParams m_params;       //转码参数

    //根据当前格式启用/禁用音视频参数区(仅.webm可操作，其余灰显)
    void updateFormatDependentState();

    // 初始化视频参数区的条件状态
    void initVideoParamsState();
    // 初始化音频参数区的条件状态
    void initAudioParamsState();
    // 更新音频质量/码率 StackWidget 的显示页面
    void updateAudioStackPage();
};

#endif // OUTPUT_SETTING_DLOG_H
