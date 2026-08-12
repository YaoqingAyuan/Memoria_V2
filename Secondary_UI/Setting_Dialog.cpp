#include "Setting_Dialog.h"
#include "ui_Setting_Dialog.h"
#include "../Core/DataModel.h"
#include "../Core/CacheManager.h"
#include <QMessageBox>

Setting_Dialog::Setting_Dialog(DataModel *model, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Setting_Dialog)
    , m_model(model)
{
    ui->setupUi(this);

    //Tab1：列头设置
    syncCheckboxesFromSettings();

    //Tab2：缓存管理
    //填充过期天数下拉（1-30天）
    for (int i = 1; i <= 30; ++i)
        ui->combo_expiryDays->addItem(QString::number(i), i);
    loadCacheSettings();
    refreshCacheSize();

    //Tab2信号槽连接
    connect(ui->cb_cleanOnClose, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        onCleanOnCloseChanged(static_cast<int>(state));
    });
    connect(ui->btn_cleanCacheNow, &QPushButton::clicked, this, &Setting_Dialog::onCleanCacheNowClicked);
    connect(ui->combo_expiryDays, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Setting_Dialog::onExpiryDaysChanged);
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

// ============================================================
// Tab2：缓存管理
// ============================================================

//加载缓存设置到UI
void Setting_Dialog::loadCacheSettings()
{
    auto &cm = CacheManager::instance();

    //关闭时清理
    bool cleanOnClose = cm.cleanOnClose();
    ui->cb_cleanOnClose->setChecked(cleanOnClose);

    //过期天数
    int days = cm.expiryDays();
    int index = ui->combo_expiryDays->findData(days);
    if (index >= 0)
        ui->combo_expiryDays->setCurrentIndex(index);

    //根据CheckBox状态启用/禁用下拉
    ui->combo_expiryDays->setEnabled(!cleanOnClose);
    ui->label_expiry->setEnabled(!cleanOnClose);
    ui->label_expirySuffix->setEnabled(!cleanOnClose);
    ui->label_expiryHint->setEnabled(!cleanOnClose);
}

//刷新缓存大小显示
void Setting_Dialog::refreshCacheSize()
{
    qint64 size = CacheManager::instance().cacheSize();
    ui->label_cacheSize->setText(
        QStringLiteral("当前缓存: %1").arg(CacheManager::formatSize(size)));
}

//关闭时清理CheckBox状态变化
void Setting_Dialog::onCleanOnCloseChanged(int state)
{
    bool checked = (state == Qt::Checked);
    CacheManager::instance().setCleanOnClose(checked);

    //勾选时禁用过期天数选项（关闭时已全量清理，无需过期策略）
    ui->combo_expiryDays->setEnabled(!checked);
    ui->label_expiry->setEnabled(!checked);
    ui->label_expirySuffix->setEnabled(!checked);
    ui->label_expiryHint->setEnabled(!checked);
}

//手动清理全部缓存
void Setting_Dialog::onCleanCacheNowClicked()
{
    auto &cm = CacheManager::instance();
    qint64 beforeSize = cm.cacheSize();
    if (beforeSize == 0) {
        QMessageBox::information(this, QStringLiteral("缓存清理"),
            QStringLiteral("缓存为空，无需清理。"));
        return;
    }

    auto ret = QMessageBox::question(this, QStringLiteral("确认清理"),
        QStringLiteral("将清理全部缓存（%1），此操作不可撤销。\n确定继续？")
            .arg(CacheManager::formatSize(beforeSize)),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    cm.cleanAll();
    refreshCacheSize();
    QMessageBox::information(this, QStringLiteral("清理完成"),
        QStringLiteral("已释放 %1 缓存空间。")
            .arg(CacheManager::formatSize(beforeSize)));
}

//过期天数变化
void Setting_Dialog::onExpiryDaysChanged(int index)
{
    int days = ui->combo_expiryDays->itemData(index).toInt();
    CacheManager::instance().setExpiryDays(days);
}
