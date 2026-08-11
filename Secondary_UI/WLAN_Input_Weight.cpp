#include "WLAN_Input_Weight.h"
#include "ui_WLAN_Input_Weight.h"

#include <QTreeWidgetItemIterator>
#include <QMessageBox>
#include <QHeaderView>
#include <QColor>
#include <QMenu>
#include <QAction>

//辅助：从树节点显示文本中提取文件夹名
//显示格式 "📁 folderName/" → 返回 "folderName"
static QString extractFolderName(const QString &displayText)
{
    QString name = displayText;
    int spaceIdx = name.indexOf(' ');
    if (spaceIdx >= 0) name = name.mid(spaceIdx + 1);
    if (name.endsWith('/')) name.chop(1);
    return name.trimmed();
}

WLAN_Input_Weight::WLAN_Input_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WLAN_Input_Weight)
{
    ui->setupUi(this);
    initUI();
    populateDemoData();
}

WLAN_Input_Weight::~WLAN_Input_Weight()
{
    delete ui;
}

//==================================================
// initUI: .ui无法表达的运行时配置（分割比例、表头模式、信号槽）
//==================================================
void WLAN_Input_Weight::initUI()
{
    //三栏比例：左固定 / 中弹性 / 右固定
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setStretchFactor(2, 0);
    ui->splitter->setSizes({180, 460, 260});
    //主布局中splitter占据弹性空间（索引1）
    ui->mainLayout->setStretchFactor(ui->splitter, 1);

    //文件树列宽模式
    ui->fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->fileTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    //预览树列宽模式
    ui->previewTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->previewTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    //=== 信号/槽连接 ===
    connect(ui->refreshBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onRefreshDevices);
    connect(ui->deviceList, &QListWidget::customContextMenuRequested,
            this, &WLAN_Input_Weight::onDeviceContextMenu);
    connect(ui->fileTree, &QTreeWidget::itemChanged,
            this, &WLAN_Input_Weight::onFileItemChanged);
    connect(ui->parseBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onParseSelected);
    connect(ui->confirmBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onConfirmImport);
    connect(ui->selectAllCheck, &QCheckBox::toggled,
            this, &WLAN_Input_Weight::onSelectAllChanged);
    connect(ui->previewTree, &QTreeWidget::itemChanged,
            this, &WLAN_Input_Weight::onPreviewItemChanged);
    connect(ui->wifiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WLAN_Input_Weight::onWifiSelectionChanged);
    connect(ui->connectBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onConnectWifi);
    connect(ui->upBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onUpButtonClicked);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &QWidget::close);
}

//==================================================
// populateDemoData: 填充演示数据（WiFi、设备、文件树）
//==================================================
void WLAN_Input_Weight::populateDemoData()
{
    //WiFi 网络列表
    ui->wifiCombo->addItem(QStringLiteral("选择WiFi网络..."));
    ui->wifiCombo->addItem("📶 CU_7pfc_5G  🔒");
    ui->wifiCombo->addItem("📶 OnePlus Ace 3V  🔒");
    ui->wifiCombo->addItem("📶 PMLK");

    //设备列表（不显示IP；IP存储在UserRole中供右键菜单查看）
    auto *dev1 = new QListWidgetItem(QStringLiteral("📱  Pixel 7 Pro  · 已连接"));
    dev1->setData(Qt::UserRole, "192.168.1.8");
    dev1->setBackground(QColor(229, 234, 255));  //#E5EAFF 高亮已连接设备
    ui->deviceList->addItem(dev1);

    auto *dev2 = new QListWidgetItem(QStringLiteral("📱  iPhone 13  · 在线"));
    dev2->setData(Qt::UserRole, "192.168.1.5");
    ui->deviceList->addItem(dev2);

    auto *searching = new QListWidgetItem(QStringLiteral("🔍  搜索中..."));
    searching->setFlags(searching->flags() & ~Qt::ItemIsSelectable);
    searching->setForeground(QColor(82, 82, 91));  //#52525B
    ui->deviceList->addItem(searching);

    //文件树（缓存文件结构，依据 CatheFile_tree.txt）
    ui->fileTree->blockSignals(true);

    //download/
    auto *download = new QTreeWidgetItem({"📁 download/", ""});
    ui->fileTree->addTopLevelItem(download);

    //328668592/ (离线诊断ID, 多P示例)
    auto *id1 = new QTreeWidgetItem({"📁 328668592/", ""});
    id1->setCheckState(0, Qt::Checked);
    download->addChild(id1);

    //c_205344854/ (P1)
    auto *c1 = new QTreeWidgetItem({"📁 c_205344854/", ""});
    id1->addChild(c1);
    c1->addChild(new QTreeWidgetItem({"📄 cover.jpg", "125 KB"}));
    c1->addChild(new QTreeWidgetItem({"📄 danmaku.xml", "15 KB"}));
    c1->addChild(new QTreeWidgetItem({"📄 danmaku.pb", "45 KB"}));
    c1->addChild(new QTreeWidgetItem({"📄 entry.json", "2.1 KB"}));
    auto *dir80_1 = new QTreeWidgetItem({"📁 80/", ""});
    c1->addChild(dir80_1);
    dir80_1->addChild(new QTreeWidgetItem({"📄 audio.m4s", "32.1 MB"}));
    dir80_1->addChild(new QTreeWidgetItem({"📄 index.json", "1.5 KB"}));
    dir80_1->addChild(new QTreeWidgetItem({"📄 video.m4s", "45.2 MB"}));

    //c_205348377/ (P2)
    auto *c2 = new QTreeWidgetItem({"📁 c_205348377/", ""});
    id1->addChild(c2);
    c2->addChild(new QTreeWidgetItem({"📄 cover.jpg", "130 KB"}));
    c2->addChild(new QTreeWidgetItem({"📄 entry.json", "2.3 KB"}));
    auto *dir80_2 = new QTreeWidgetItem({"📁 80/", ""});
    c2->addChild(dir80_2);
    dir80_2->addChild(new QTreeWidgetItem({"📄 audio.m4s", "28.5 MB"}));
    dir80_2->addChild(new QTreeWidgetItem({"📄 video.m4s", "38.7 MB"}));

    //S_46752/ (离线诊断ID, 番剧示例)
    auto *id2 = new QTreeWidgetItem({"📁 S_46752/", ""});
    id2->setCheckState(0, Qt::Unchecked);
    download->addChild(id2);

    //801229/ (第1话)
    auto *ep1 = new QTreeWidgetItem({"📁 801229/", ""});
    id2->addChild(ep1);
    ep1->addChild(new QTreeWidgetItem({"📄 cover.jpg", "110 KB"}));
    ep1->addChild(new QTreeWidgetItem({"📄 entry.json", "1.8 KB"}));
    auto *dir80_3 = new QTreeWidgetItem({"📁 80/", ""});
    ep1->addChild(dir80_3);
    dir80_3->addChild(new QTreeWidgetItem({"📄 audio.m4s", "25.3 MB"}));
    dir80_3->addChild(new QTreeWidgetItem({"📄 video.m4s", "42.1 MB"}));

    //801230/ (第2话, 折叠)
    auto *ep2 = new QTreeWidgetItem({"📁 801230/", ""});
    id2->addChild(ep2);

    //.patch/
    auto *patch = new QTreeWidgetItem({"📁 .patch/", ""});
    download->addChild(patch);

    ui->fileTree->blockSignals(false);

    //展开前两级
    download->setExpanded(true);
    id1->setExpanded(true);
    id2->setExpanded(false);
}

//==================================================
// 槽函数实现
//==================================================

void WLAN_Input_Weight::onRefreshDevices()
{
    //TODO: 接入 WlanManager::start() 重新广播并扫描
    QMessageBox::information(this, QStringLiteral("刷新设备"),
                             QStringLiteral("正在搜索同一局域网内的设备..."));
}

void WLAN_Input_Weight::onDeviceContextMenu(const QPoint &pos)
{
    auto *item = ui->deviceList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *detailAction = menu.addAction(QStringLiteral("设备详情"));

    QAction *selected = menu.exec(ui->deviceList->mapToGlobal(pos));
    if (selected == detailAction) {
        QString ip = item->data(Qt::UserRole).toString();
        QString text = item->text();
        //提取设备名（去掉emoji和状态后缀）
        QString name = text;
        int dotIdx = name.indexOf(QChar(0x00B7));  //"·" 分隔符
        if (dotIdx > 0) name = name.left(dotIdx).trimmed();
        int spaceIdx = name.indexOf(' ');
        if (spaceIdx >= 0) name = name.mid(spaceIdx + 1).trimmed();

        QMessageBox::information(this, QStringLiteral("设备详情"),
            QStringLiteral("设备名: %1\nIP 地址: %2").arg(name, ip));
    }
}

void WLAN_Input_Weight::onFileItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0 || item->checkState(0) != Qt::Checked) return;

    //防呆验证：只有"离线诊断ID"文件夹可被选中
    //特征：纯数字(如328668592) 或 S_开头(如S_46752)
    QString folderName = extractFolderName(item->text(0));

    bool isNumeric = false;
    folderName.toLongLong(&isNumeric);
    bool isBangumi = folderName.startsWith("S_");

    if (!isNumeric && !isBangumi) {
        ui->fileTree->blockSignals(true);
        item->setCheckState(0, Qt::Unchecked);
        ui->fileTree->blockSignals(false);

        QMessageBox::warning(this, QStringLiteral("选择无效"),
            QStringLiteral("请选择\"离线诊断ID\"文件夹（纯数字或 S_ 开头），\n"
                           "而非内部子目录或文件。"));
    }
}

void WLAN_Input_Weight::onParseSelected()
{
    //收集所有勾选的离线诊断ID文件夹
    QStringList checkedIds;
    QTreeWidgetItemIterator it(ui->fileTree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->checkState(0) == Qt::Checked) {
            checkedIds << extractFolderName(item->text(0));
        }
        ++it;
    }

    if (checkedIds.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无可解析文件"),
            QStringLiteral("请先在文件浏览中勾选\"离线诊断ID\"文件夹。"));
        return;
    }

    //TODO: 对已传输到本地的文件调用 CacheFileParser::Cathe_Parse()
    //暂时用Demo数据填充预览树：每个ID生成一个可展开的"菜单列表"
    ui->previewTree->blockSignals(true);
    ui->previewTree->clear();

    for (const QString &id : checkedIds) {
        auto *folderItem = new QTreeWidgetItem({"📁 " + id, ""});
        folderItem->setCheckState(0, Qt::Unchecked);

        //Demo数据：根据离线诊断ID显示不同信息
        if (id == "328668592") {
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("标题"), QStringLiteral("【测试】示例视频标题")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("UP主"), QStringLiteral("测试用户")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("Bv号"), "BV1xx411c7mD"}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("分P"), QStringLiteral("第1页 - 正片 / 第2页 - 第二P")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("大小"), "154.6 MB"}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("状态"), QStringLiteral("✓ 已解析 (2P)")}));
        } else if (id == "S_46752") {
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("标题"), QStringLiteral("我们渡江了 第1季")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("UP主"), QStringLiteral("番剧官方")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("Bv号"), "—"}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("分P"), QStringLiteral("第1话 - 渡江 / 第2话 - 到达")}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("大小"), "202.4 MB"}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("状态"), QStringLiteral("✓ 已解析 (2话)")}));
        } else {
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("标题"), id}));
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("状态"), QStringLiteral("✓ 已解析")}));
        }

        ui->previewTree->addTopLevelItem(folderItem);
    }

    ui->previewTree->blockSignals(false);

    //重置全选状态
    ui->selectAllCheck->blockSignals(true);
    ui->selectAllCheck->setChecked(false);
    ui->selectAllCheck->blockSignals(false);
}

void WLAN_Input_Weight::onConfirmImport()
{
    //收集预览树中所有勾选的离线诊断ID
    QStringList selectedIds;
    for (int i = 0; i < ui->previewTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->previewTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            selectedIds << extractFolderName(item->text(0));
        }
    }

    if (selectedIds.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("未选择"),
            QStringLiteral("请在解析预览中勾选要导入的\"离线诊断ID\"，\n"
                           "或点击\"全选\"选择全部。"));
        return;
    }

    //TODO: 将解析好的 QList<ParsedCacheData> 通过信号发给 MainWindow
    emit importConfirmed();
    close();
}

void WLAN_Input_Weight::onSelectAllChanged(bool checked)
{
    //全选/全不选：同步所有顶层项的勾选状态
    ui->previewTree->blockSignals(true);
    Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    for (int i = 0; i < ui->previewTree->topLevelItemCount(); ++i) {
        ui->previewTree->topLevelItem(i)->setCheckState(0, state);
    }
    ui->previewTree->blockSignals(false);
}

void WLAN_Input_Weight::onPreviewItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0) return;

    //只处理顶层项（离线诊断ID文件夹）的勾选状态变化
    if (ui->previewTree->indexOfTopLevelItem(item) < 0) return;

    //根据顶层项勾选情况更新"全选"复选框（全部勾选时才选中）
    int total = ui->previewTree->topLevelItemCount();
    int checkedCount = 0;
    for (int i = 0; i < total; ++i) {
        if (ui->previewTree->topLevelItem(i)->checkState(0) == Qt::Checked)
            ++checkedCount;
    }

    ui->selectAllCheck->blockSignals(true);
    ui->selectAllCheck->setChecked(checkedCount == total && total > 0);
    ui->selectAllCheck->blockSignals(false);
}

void WLAN_Input_Weight::onWifiSelectionChanged(int index)
{
    if (index <= 0) {
        //"选择WiFi网络..." — 隐藏密码和连接按钮
        ui->passwordEdit->hide();
        ui->connectBtn->hide();
        return;
    }

    QString text = ui->wifiCombo->itemText(index);
    bool secured = text.contains("🔒");

    if (secured) {
        ui->passwordEdit->show();
        ui->connectBtn->show();
        ui->connectBtn->setText(QStringLiteral("连接"));
    } else {
        //开放网络：无需密码
        ui->passwordEdit->hide();
        ui->connectBtn->show();
        ui->connectBtn->setText(QStringLiteral("连接"));
    }
}

void WLAN_Input_Weight::onConnectWifi()
{
    //TODO: 实际WiFi连接逻辑（调用系统API或 WlanManager）
    QString network = ui->wifiCombo->currentText();
    QMessageBox::information(this, QStringLiteral("连接WiFi"),
        QStringLiteral("正在连接到 %1 ...").arg(network));
}

void WLAN_Input_Weight::onUpButtonClicked()
{
    //TODO: 向远程设备发送 RequestDirList 请求上级路径
    QMessageBox::information(this, QStringLiteral("上级目录"),
                             QStringLiteral("TODO: 返回上级目录"));
}
