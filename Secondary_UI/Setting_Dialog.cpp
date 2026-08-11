#include "Setting_Dialog.h"
#include "ui_Setting_Dialog.h"
#include "../Core/DataModel.h"

Setting_Dialog::Setting_Dialog(DataModel *model, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Setting_Dialog)
    , m_model(model)
{
    ui->setupUi(this);
    //从配置加载当前列可见性状态到复选框
    syncCheckboxesFromSettings();
}

Setting_Dialog::~Setting_Dialog()
{
    delete ui;
}

//应用按钮：将复选框状态写入DataModel(会自动保存到配置并刷新表格)
void Setting_Dialog::on_applyBtn_clicked()
{
    if (!m_model)
        return;

    //逐个读取复选框状态，写入DataModel
    m_model->setOptionalColumnVisible(DataModel::ColAvid,         ui->cb_avid->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColBvid,         ui->cb_bvid->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColVideoType,    ui->cb_videoType->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColOwnerName,    ui->cb_ownerName->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColOwnerId,      ui->cb_ownerId->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColCreateTime,   ui->cb_createTime->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColDuration,     ui->cb_duration->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColFileSize,     ui->cb_fileSize->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColDanmakuCount, ui->cb_danmakuCount->isChecked());
    m_model->setOptionalColumnVisible(DataModel::ColResolution,   ui->cb_resolution->isChecked());
}

//关闭按钮：直接关闭对话框
void Setting_Dialog::on_closeBtn_clicked()
{
    close();
}

//从DataModel读取当前列可见性，同步到复选框
void Setting_Dialog::syncCheckboxesFromModel()
{
    if (!m_model)
        return;

    ui->cb_avid->setChecked(m_model->isOptionalColumnVisible(DataModel::ColAvid));
    ui->cb_bvid->setChecked(m_model->isOptionalColumnVisible(DataModel::ColBvid));
    ui->cb_videoType->setChecked(m_model->isOptionalColumnVisible(DataModel::ColVideoType));
    ui->cb_ownerName->setChecked(m_model->isOptionalColumnVisible(DataModel::ColOwnerName));
    ui->cb_ownerId->setChecked(m_model->isOptionalColumnVisible(DataModel::ColOwnerId));
    ui->cb_createTime->setChecked(m_model->isOptionalColumnVisible(DataModel::ColCreateTime));
    ui->cb_duration->setChecked(m_model->isOptionalColumnVisible(DataModel::ColDuration));
    ui->cb_fileSize->setChecked(m_model->isOptionalColumnVisible(DataModel::ColFileSize));
    ui->cb_danmakuCount->setChecked(m_model->isOptionalColumnVisible(DataModel::ColDanmakuCount));
    ui->cb_resolution->setChecked(m_model->isOptionalColumnVisible(DataModel::ColResolution));
}

//从配置读取列可见性，同步到复选框
void Setting_Dialog::syncCheckboxesFromSettings()
{
    QSettings settings;
    //DataModel的默认值定义在s_defaultVisible中，这里直接从配置读取
    //若配置中不存在则使用DataModel的默认值
    ui->cb_avid->setChecked(settings.value("Table/OptionalCol_0", true).toBool());
    ui->cb_bvid->setChecked(settings.value("Table/OptionalCol_1", false).toBool());
    ui->cb_videoType->setChecked(settings.value("Table/OptionalCol_2", false).toBool());
    ui->cb_ownerName->setChecked(settings.value("Table/OptionalCol_3", true).toBool());
    ui->cb_ownerId->setChecked(settings.value("Table/OptionalCol_4", false).toBool());
    ui->cb_createTime->setChecked(settings.value("Table/OptionalCol_5", false).toBool());
    ui->cb_duration->setChecked(settings.value("Table/OptionalCol_6", false).toBool());
    ui->cb_fileSize->setChecked(settings.value("Table/OptionalCol_7", false).toBool());
    ui->cb_danmakuCount->setChecked(settings.value("Table/OptionalCol_8", false).toBool());
    ui->cb_resolution->setChecked(settings.value("Table/OptionalCol_9", true).toBool());
}
