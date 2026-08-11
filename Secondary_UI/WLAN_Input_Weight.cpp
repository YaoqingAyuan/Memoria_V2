#include "WLAN_Input_Weight.h"
#include "ui_WLAN_Input_Weight.h"

#include "ADB_Module/AdbModule.h"
#include "Parser_Module/CacheFileParser.h"
#include "Core/ParsedCacheData.h"
#include "Core/logger.h"

#include <QStandardItemModel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QColor>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QFileInfo>

//B站缓存默认根路径
const QString WLAN_Input_Weight::BILI_CACHE_ROOT = "/sdcard/Android/data/tv.danmaku.bili/download";

//辅助：格式化文件大小
static QString formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
}

//辅助：递归计算目录总大小
static qint64 dirSize(const QString &path)
{
    qint64 total = 0;
    QDir dir(path);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (entry.isDir()) total += dirSize(entry.absoluteFilePath());
        else total += entry.size();
    }
    return total;
}

//辅助：从预览树节点显示文本中提取文件夹名
//显示格式 "📁 folderName/" → 返回 "folderName"
static QString extractFolderName(const QString &displayText)
{
    QString name = displayText;
    int spaceIdx = name.indexOf(' ');
    if (spaceIdx >= 0) name = name.mid(spaceIdx + 1);
    if (name.endsWith('/')) name.chop(1);
    return name.trimmed();
}

// ============================================================
// 构造/析构
// ============================================================

WLAN_Input_Weight::WLAN_Input_Weight(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WLAN_Input_Weight)
    , m_adb(new AdbModule(this))
    , m_fileModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    initUI();

    //ADB自检
    QString adbPath = m_adb->selfCheck();
    updateAdbStatus(!adbPath.isEmpty(),
                    adbPath.isEmpty() ? QStringLiteral("ADB: 未找到") : QStringLiteral("ADB: 就绪"));

    //设置本地拉取暂存目录
    m_localPullDir = QDir::tempPath() + "/Memoria_ADB_Pull";
    QDir().mkpath(m_localPullDir);

    //自检通过则自动刷新设备列表
    if (!adbPath.isEmpty()) {
        onRefreshDevices();
    }
}

WLAN_Input_Weight::~WLAN_Input_Weight()
{
    delete ui;
}

// ============================================================
// initUI: .ui无法表达的运行时配置
// ============================================================

void WLAN_Input_Weight::initUI()
{
    //三栏比例：左固定 / 中弹性 / 右固定
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setStretchFactor(2, 0);
    ui->splitter->setSizes({180, 460, 260});
    ui->mainLayout->setStretchFactor(ui->splitter, 1);

    //文件列表模型
    ui->fileList->setModel(m_fileModel);

    //预览树列宽模式
    ui->previewTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->previewTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    //进度条初始状态
    ui->progressBar->setValue(0);
    ui->progressLabel->setText(QStringLiteral("就绪"));

    //路径标签初始显示
    ui->pathLabel->setText(BILI_CACHE_ROOT);

    //=== UI信号/槽连接 ===
    //ADB设备管理
    connect(ui->refreshBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onRefreshDevices);
    connect(ui->pairBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onPairDevice);
    connect(ui->connectDeviceBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onConnectDevice);
    connect(ui->deviceList, &QListWidget::customContextMenuRequested,
            this, &WLAN_Input_Weight::onDeviceContextMenu);
    connect(ui->deviceList, &QListWidget::itemSelectionChanged,
            this, &WLAN_Input_Weight::onDeviceSelectionChanged);

    //远程文件浏览
    connect(ui->fileList, &QListView::doubleClicked,
            this, &WLAN_Input_Weight::onFileDoubleClicked);
    connect(ui->upBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onUpButtonClicked);

    //拉取/解析
    connect(ui->parseBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onParseSelected);

    //预览/导入
    connect(ui->confirmBtn, &QPushButton::clicked, this, &WLAN_Input_Weight::onConfirmImport);
    connect(ui->selectAllCheck, &QCheckBox::toggled, this, &WLAN_Input_Weight::onSelectAllChanged);
    connect(ui->previewTree, &QTreeWidget::itemChanged, this, &WLAN_Input_Weight::onPreviewItemChanged);

    //取消
    connect(ui->cancelBtn, &QPushButton::clicked, this, &QWidget::close);

    //=== AdbModule信号连接 ===
    connect(m_adb, &AdbModule::deviceListChanged, this, &WLAN_Input_Weight::onDeviceListChanged);
    connect(m_adb, &AdbModule::dirListReady, this, &WLAN_Input_Weight::onDirListReady);
    connect(m_adb, &AdbModule::pullProgressChanged, this, &WLAN_Input_Weight::onPullProgressChanged);
    connect(m_adb, &AdbModule::pullFinished, this, &WLAN_Input_Weight::onPullFinished);
    connect(m_adb, &AdbModule::errorOccurred, this, &WLAN_Input_Weight::onAdbError);

    //配对/连接结果（简单弹窗反馈）
    connect(m_adb, &AdbModule::pairResult, this,
            [this](bool success, const QString &msg) {
                QMessageBox::information(this, QStringLiteral("配对结果"), msg);
                if (success) onRefreshDevices();
            });
    connect(m_adb, &AdbModule::connectResult, this,
            [this](bool success, const QString &msg) {
                QMessageBox::information(this, QStringLiteral("连接结果"), msg);
                if (success) onRefreshDevices();
            });
}

// ============================================================
// ADB状态更新
// ============================================================

void WLAN_Input_Weight::updateAdbStatus(bool ready, const QString &text)
{
    ui->adbStatusLabel->setText(text);
    if (ready) {
        ui->statusDot->setStyleSheet("background-color: #1DC981; border-radius: 4px;");
        ui->statusText->setText(QStringLiteral("就绪"));
    } else {
        ui->statusDot->setStyleSheet("background-color: #EF4444; border-radius: 4px;");
        ui->statusText->setText(QStringLiteral("未就绪"));
    }
}

// ============================================================
// ADB设备管理
// ============================================================

void WLAN_Input_Weight::onRefreshDevices()
{
    if (m_adb->getAdbPath().isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("ADB未就绪"),
            QStringLiteral("未检测到ADB环境，无法刷新设备列表。"));
        return;
    }
    ui->refreshBtn->setEnabled(false);
    m_adb->refreshDevices();
}

void WLAN_Input_Weight::onPairDevice()
{
    //配对需要两步输入：配对地址(IP:端口) + 配对码
    bool ok;
    QString addr = QInputDialog::getText(this, QStringLiteral("配对设备"),
        QStringLiteral("配对地址 (IP:端口):"), QLineEdit::Normal, "", &ok);
    if (!ok || addr.trimmed().isEmpty()) return;

    QString code = QInputDialog::getText(this, QStringLiteral("配对设备"),
        QStringLiteral("配对码 (6位数字):"), QLineEdit::Normal, "", &ok);
    if (!ok || code.trimmed().isEmpty()) return;

    QStringList parts = addr.trimmed().split(':');
    if (parts.size() != 2) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("地址格式应为 IP:端口，如 192.168.1.8:42171"));
        return;
    }

    m_adb->pairDevice(parts[0], parts[1].toInt(), code.trimmed());
}

void WLAN_Input_Weight::onConnectDevice()
{
    QString text = ui->connectEdit->text().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入为空"),
            QStringLiteral("请输入设备地址，如 192.168.1.8:5555"));
        return;
    }

    QStringList parts = text.split(':');
    if (parts.size() != 2) {
        QMessageBox::warning(this, QStringLiteral("格式错误"),
            QStringLiteral("地址格式应为 IP:端口，如 192.168.1.8:5555"));
        return;
    }

    m_adb->connectDevice(parts[0], parts[1].toInt());
}

void WLAN_Input_Weight::onDeviceListChanged(const QList<AdbDeviceInfo> &devices)
{
    ui->deviceList->blockSignals(true);
    ui->deviceList->clear();

    for (const auto &dev : devices) {
        QString displayText = dev.model.isEmpty()
            ? (QStringLiteral("📱 ") + dev.serial)
            : (QStringLiteral("📱 ") + dev.model);

        if (dev.state == "device") {
            displayText += QStringLiteral(" · 已连接");
        } else if (dev.state == "offline") {
            displayText += QStringLiteral(" · 离线");
        } else if (dev.state == "unauthorized") {
            displayText += QStringLiteral(" · 未授权");
        }

        auto *item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, dev.serial);
        if (dev.state == "device") {
            item->setBackground(QColor(229, 234, 255));  //高亮已连接设备
        }
        ui->deviceList->addItem(item);
    }

    if (devices.isEmpty()) {
        auto *empty = new QListWidgetItem(QStringLiteral("🔍 暂无设备，请配对/连接后刷新"));
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        empty->setForeground(QColor(82, 82, 91));
        ui->deviceList->addItem(empty);
    }

    ui->deviceList->blockSignals(false);
    ui->refreshBtn->setEnabled(true);
}

void WLAN_Input_Weight::onDeviceSelectionChanged()
{
    auto *item = ui->deviceList->currentItem();
    if (!item) return;

    m_currentSerial = item->data(Qt::UserRole).toString();
    if (m_currentSerial.isEmpty()) return;

    //选中设备后自动浏览B站缓存目录
    browseDir(m_currentRemotePath.isEmpty() ? BILI_CACHE_ROOT : m_currentRemotePath);
}

void WLAN_Input_Weight::onDeviceContextMenu(const QPoint &pos)
{
    auto *item = ui->deviceList->itemAt(pos);
    if (!item) return;

    QString serial = item->data(Qt::UserRole).toString();
    if (serial.isEmpty()) return;

    QMenu menu(this);
    QAction *detailAction = menu.addAction(QStringLiteral("设备详情"));
    QAction *disconnectAction = menu.addAction(QStringLiteral("断开连接"));

    QAction *selected = menu.exec(ui->deviceList->mapToGlobal(pos));
    if (selected == detailAction) {
        QMessageBox::information(this, QStringLiteral("设备详情"),
            QStringLiteral("序列号: %1\n显示: %2").arg(serial, item->text()));
    } else if (selected == disconnectAction) {
        m_adb->disconnectDevice(serial);
    }
}

// ============================================================
// 远程文件浏览
// ============================================================

void WLAN_Input_Weight::browseDir(const QString &remotePath)
{
    if (m_currentSerial.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("未选择设备"),
            QStringLiteral("请先在设备列表中选择一台已连接的设备。"));
        return;
    }
    m_currentRemotePath = remotePath;
    ui->pathLabel->setText(remotePath);
    m_adb->listDir(m_currentSerial, remotePath);
}

void WLAN_Input_Weight::onDirListReady(const QString &path, const QList<AdbDirEntry> &entries)
{
    Q_UNUSED(path)
    m_fileModel->clear();

    for (const auto &entry : entries) {
        QString displayText;
        if (entry.isDir) {
            displayText = QStringLiteral("📁 ") + entry.name + "/";
        } else {
            displayText = QStringLiteral("📄 ") + entry.name + "  (" + formatSize(entry.size) + ")";
        }

        auto *item = new QStandardItem(displayText);
        item->setData(entry.name, Qt::UserRole);        //原始文件名
        item->setData(entry.isDir, Qt::UserRole + 1);   //是否为目录
        item->setEditable(false);
        m_fileModel->appendRow(item);
    }
}

void WLAN_Input_Weight::onFileDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QStandardItem *item = m_fileModel->item(index.row());
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString name = item->data(Qt::UserRole).toString();

    if (isDir) {
        browseDir(m_currentRemotePath + "/" + name);
    }
}

void WLAN_Input_Weight::onUpButtonClicked()
{
    if (m_currentRemotePath.isEmpty() || m_currentRemotePath == "/") return;

    int lastSlash = m_currentRemotePath.lastIndexOf('/');
    if (lastSlash <= 0) {
        browseDir("/");
    } else {
        browseDir(m_currentRemotePath.left(lastSlash));
    }
}

// ============================================================
// 拉取 + 解析
// ============================================================

void WLAN_Input_Weight::onParseSelected()
{
    if (m_currentSerial.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("未选择设备"),
            QStringLiteral("请先在设备列表中选择一台已连接的设备。"));
        return;
    }

    //收集选中的目录
    QStringList selectedPaths;
    const auto selectedIndexes = ui->fileList->selectionModel()->selectedRows();
    for (const auto &index : selectedIndexes) {
        QStandardItem *item = m_fileModel->item(index.row());
        if (!item) continue;

        bool isDir = item->data(Qt::UserRole + 1).toBool();
        QString name = item->data(Qt::UserRole).toString();
        if (isDir) {
            selectedPaths << (m_currentRemotePath + "/" + name);
        }
    }

    if (selectedPaths.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("未选择文件夹"),
            QStringLiteral("请先在文件浏览中选择一个或多个文件夹。\n"
                           "（选中离线诊断ID文件夹后点击解析）"));
        return;
    }

    //初始化拉取队列
    m_pullQueue = selectedPaths;
    m_pullTotal = selectedPaths.size();
    m_pullCompleted = 0;

    //清空预览树
    ui->previewTree->blockSignals(true);
    ui->previewTree->clear();
    ui->previewTree->blockSignals(false);
    ui->selectAllCheck->blockSignals(true);
    ui->selectAllCheck->setChecked(false);
    ui->selectAllCheck->blockSignals(false);

    //开始拉取
    ui->parseBtn->setEnabled(false);
    startNextPull();
}

void WLAN_Input_Weight::startNextPull()
{
    if (m_pullQueue.isEmpty()) {
        //全部完成
        ui->progressBar->setValue(100);
        ui->progressLabel->setText(
            QStringLiteral("完成：已解析 %1/%2").arg(m_pullCompleted).arg(m_pullTotal));
        ui->parseBtn->setEnabled(true);
        return;
    }

    QString remotePath = m_pullQueue.takeFirst();
    QString folderName = remotePath.section('/', -1);
    QString localPath = m_localPullDir + "/" + folderName;

    ui->progressLabel->setText(
        QStringLiteral("拉取中 (%1/%2): %3").arg(m_pullCompleted + 1).arg(m_pullTotal).arg(folderName));

    m_adb->pullFile(m_currentSerial, remotePath, localPath);
}

void WLAN_Input_Weight::onPullProgressChanged(int percentage)
{
    ui->progressBar->setValue(percentage);
}

void WLAN_Input_Weight::onPullFinished(const QString &localPath, bool success, const QString &message)
{
    m_pullCompleted++;

    if (success) {
        parsePulledFolder(localPath);
    } else {
        Logger::instance()->warning("WLAN_Input", QString("拉取失败: %1").arg(message));
        //在预览树中显示失败项
        QString folderName = QFileInfo(localPath).fileName();
        ui->previewTree->blockSignals(true);
        auto *failItem = new QTreeWidgetItem({"📁 " + folderName, ""});
        failItem->setCheckState(0, Qt::Unchecked);
        failItem->addChild(new QTreeWidgetItem({QStringLiteral("状态"), QStringLiteral("❌ 拉取失败")}));
        ui->previewTree->addTopLevelItem(failItem);
        ui->previewTree->blockSignals(false);
    }

    startNextPull();
}

void WLAN_Input_Weight::parsePulledFolder(const QString &localPath)
{
    CacheFileParser parser;
    QList<ParsedCacheData> parsedList;
    bool ok = parser.Cathe_Parse(localPath, parsedList);

    QString folderName = QFileInfo(localPath).fileName();

    ui->previewTree->blockSignals(true);

    auto *folderItem = new QTreeWidgetItem({"📁 " + folderName, ""});
    folderItem->setCheckState(0, Qt::Unchecked);

    if (ok && !parsedList.isEmpty()) {
        //多P场景：取第一个的有效信息作为标题/UP主/Bv号（同一视频各P信息一致）
        const auto &first = parsedList.first();
        folderItem->addChild(new QTreeWidgetItem({QStringLiteral("标题"), first.videoInfo.title}));
        folderItem->addChild(new QTreeWidgetItem({QStringLiteral("UP主"), first.videoInfo.ownerName}));
        folderItem->addChild(new QTreeWidgetItem({QStringLiteral("Bv号"), first.videoInfo.bvid}));

        //分P信息：列出所有已解析的P
        if (parsedList.size() > 1) {
            folderItem->addChild(new QTreeWidgetItem(
                {QStringLiteral("分P"), QStringLiteral("共 %1 个分P").arg(parsedList.size())}));
        } else {
            folderItem->addChild(new QTreeWidgetItem({QStringLiteral("分P"), QStringLiteral("单P")}));
        }

        folderItem->addChild(new QTreeWidgetItem({QStringLiteral("大小"), formatSize(dirSize(localPath))}));
        folderItem->addChild(new QTreeWidgetItem(
            {QStringLiteral("状态"), QStringLiteral("✓ 已解析 (%1P)").arg(parsedList.size())}));
    } else {
        folderItem->addChild(new QTreeWidgetItem({QStringLiteral("状态"), QStringLiteral("❌ 解析失败")}));
    }

    ui->previewTree->addTopLevelItem(folderItem);
    ui->previewTree->blockSignals(false);
}

// ============================================================
// 预览/导入
// ============================================================

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

    //根据顶层项勾选情况更新"全选"复选框
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

// ============================================================
// ADB错误处理
// ============================================================

void WLAN_Input_Weight::onAdbError(const QString &errorMsg)
{
    Logger::instance()->warning("WLAN_Input", QString("ADB错误: %1").arg(errorMsg));
    QMessageBox::warning(this, QStringLiteral("ADB错误"), errorMsg);
    ui->refreshBtn->setEnabled(true);
    ui->parseBtn->setEnabled(true);
}
