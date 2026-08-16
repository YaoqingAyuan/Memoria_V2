#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "Core/DataModel.h"
#include "Core/ParsedCacheData.h"
#include "Core/CacheManager.h"
#include "TaskQueue/TaskQueue.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Parser_Module/CacheFileParser.h"
#include "Core/utils.h"
#include "Secondary_UI/Setting_Dialog.h"
#include "Secondary_UI/Independ_Import_Dialog.h"
#include "Secondary_UI/Output_Setting_Dlog.h"
#include "Secondary_UI/ExterDevice_Input_Weight.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QListView>
#include <QTreeView>
#include <QDateTime>
#include <QMessageBox>
#include <QCloseEvent>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dataModel(new DataModel(this))
    , m_taskQueue(new TaskQueue(m_dataModel, this))
{
    ui->setupUi(this);

    //启动时自检FFmpeg环境，根据结果更新状态标签(委托 TaskQueue → FFmpeg_module::selfCheck)
    //自检同步执行(内部 where ffmpeg 通常 <200ms)，结果路径同时预填到FFmpeg_module供后续混流复用
    const QString ffmpegPath = m_taskQueue->selfCheckFFmpeg();
    if (ffmpegPath.isEmpty()) {
        ui->FFmpegEnvLabel->setText(QStringLiteral("❌ 未找到 FFmpeg 环境"));
        ui->FFmpegEnvLabel->setToolTip(QStringLiteral(
            "未检测到 FFmpeg。请将 ffmpeg.exe 加入系统 PATH，或放入软件目录下的 FFmpeg_tools/bin/"));
    } else {
        ui->FFmpegEnvLabel->setText(QStringLiteral("✅ FFmpeg: %1").arg(ffmpegPath));
        ui->FFmpegEnvLabel->setToolTip(QStringLiteral("FFmpeg 路径: %1").arg(ffmpegPath));
    }

    //加载列可见性配置
    m_dataModel->loadColumnVisibility();

    //将模型绑定到表格视图
    ui->MetadataTable->setModel(m_dataModel);

    //初始化表格视觉属性
    initTable();

    //设置表格右键菜单
    setupTableContextMenu();

    //构建菜单栏Action
    setupMenuBar();

    //缓存管理：启动时清理过期缓存（崩溃恢复 + 过期清理）
    auto &cm = CacheManager::instance();
    cm.cleanExpired(cm.expiryDays());

    //单个导出任务完成 → 成功则删除对应缓存
    connect(m_taskQueue, &TaskQueue::taskFinished, this, &MainWindow::onTaskFinished);
    //总进度条：任务开始/进度更新/全部完成
    connect(m_taskQueue, &TaskQueue::taskStarted, this, &MainWindow::onTaskStarted);
    connect(m_taskQueue, &TaskQueue::taskProgress, this, &MainWindow::onTaskProgress);
    connect(m_taskQueue, &TaskQueue::allFinished, this, &MainWindow::onAllFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

//初始化表格视觉属性
void MainWindow::initTable()
{
    //选择行为：整行选中
    ui->MetadataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    //选择模式：扩展选择(支持Shift连续多选、Ctrl不连续多选，同Windows文件选择)
    ui->MetadataTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    //禁止编辑(数据通过导入/对话框修改，不直接在表格中编辑)
    ui->MetadataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //表头不拉伸(列宽由用户拖拽调整)
    ui->MetadataTable->horizontalHeader()->setStretchLastSection(false);
    //列可拖拽调整宽度
    ui->MetadataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    //行高自适应
    ui->MetadataTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    //隐藏垂直表头(行号由序号列代替)
    ui->MetadataTable->verticalHeader()->hide();
    //交替行颜色(多P分组视觉辅助)
    ui->MetadataTable->setAlternatingRowColors(true);
    //设置默认列宽
    ui->MetadataTable->horizontalHeader()->setDefaultSectionSize(120);
}

//设置表格右键菜单
void MainWindow::setupTableContextMenu()
{
    ui->MetadataTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->MetadataTable, &QTableView::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QModelIndex index = ui->MetadataTable->indexAt(pos);
        if (!index.isValid())
            return;

        //获取当前选中的所有行(支持多选批量操作)
        QModelIndexList selectedRows = ui->MetadataTable->selectionModel()->selectedRows();
        int selectedCount = selectedRows.size();
        //若右键所在行未被选中，则仅操作该行
        bool rowSelected = false;
        for (const QModelIndex &idx : selectedRows) {
            if (idx.row() == index.row()) {
                rowSelected = true;
                break;
            }
        }

        QMenu menu(this);
        QAction *indepAction = menu.addAction(QStringLiteral("独立导入…"));
        //删除菜单项文案根据选中行数动态变化
        QString deleteText = (selectedCount > 1 && rowSelected)
                             ? QStringLiteral("删除选中 %1 行").arg(selectedCount)
                             : QStringLiteral("删除此行");
        QAction *deleteAction = menu.addAction(deleteText);
        QAction *clearAction = menu.addAction(QStringLiteral("清空此行"));

        QAction *selected = menu.exec(ui->MetadataTable->viewport()->mapToGlobal(pos));

        if (selected == indepAction) {
            //弹出独立导入对话框(操作右键所在行)
            Independ_Import_Dialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                //从对话框获取路径数据，填充到右键所在行
                ParsedCacheData data;
                data.videoInfo.audioFilePath = dialog.audioPath();
                data.videoInfo.videoFilePath = dialog.videoPath();
                //标题为空时使用默认标题(视频哈希前8位_音频哈希前8位_日期)
                QString title = dialog.title();
                if (title.isEmpty()) {
                    QString videoHash = fileHashPrefix(data.videoInfo.videoFilePath, 8);
                    QString audioHash = fileHashPrefix(data.videoInfo.audioFilePath, 8);
                    title = QStringLiteral("%1_%2_%3")
                        .arg(videoHash)
                        .arg(audioHash)
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));
                }
                data.videoInfo.title = title;
                m_dataModel->setRowData(index.row(), data);
            }
        } else if (selected == deleteAction) {
            if (selectedCount > 1 && rowSelected) {
                //批量删除：收集行号降序排列，从后往前删
                QList<int> rows;
                rows.reserve(selectedRows.size());
                for (const QModelIndex &idx : selectedRows) {
                    rows.append(idx.row());
                }
                std::sort(rows.begin(), rows.end(), std::greater<int>());
                for (int r : rows) {
                    deleteCacheForRow(r);
                    m_dataModel->removeRow(r);
                }
            } else {
                //单行删除
                deleteCacheForRow(index.row());
                m_dataModel->removeRow(index.row());
            }
        } else if (selected == clearAction) {
            //重置该行为默认空行
            m_dataModel->setRowData(index.row(), ParsedCacheData());
        }
    });
}

//构建菜单栏Action(文件/编辑/视图/工具/帮助)
void MainWindow::setupMenuBar()
{
    //=== 文件(&F) ===
    auto *actImportLocal = new QAction(QStringLiteral("导入本地缓存..."), this);
    actImportLocal->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    actImportLocal->setStatusTip(QStringLiteral("从本地文件夹导入缓存数据"));
    connect(actImportLocal, &QAction::triggered, this, &MainWindow::on_LocalCache_Btn_clicked);
    ui->menu_F->addAction(actImportLocal);

    auto *actExterDevice = new QAction(QStringLiteral("外部设备导入..."), this);
    actExterDevice->setStatusTip(QStringLiteral("通过 ADB 连接外部设备导入缓存(支持 USB/WiFi)"));
    connect(actExterDevice, &QAction::triggered, this, &MainWindow::on_ExterDevice_Input_Btn_clicked);
    ui->menu_F->addAction(actExterDevice);

    ui->menu_F->addSeparator();

    auto *actIndepImport = new QAction(QStringLiteral("独立导入..."), this);
    actIndepImport->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    connect(actIndepImport, &QAction::triggered, this, &MainWindow::on_IndepImport_Btn_clicked);
    ui->menu_F->addAction(actIndepImport);

    ui->menu_F->addSeparator();

    auto *actSetOutputPath = new QAction(QStringLiteral("设置导出路径..."), this);
    connect(actSetOutputPath, &QAction::triggered, this, &MainWindow::on_OutputPath_Btn_clicked);
    ui->menu_F->addAction(actSetOutputPath);

    auto *actExport = new QAction(QStringLiteral("导出..."), this);
    actExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(actExport, &QAction::triggered, this, &MainWindow::on_OutputBtn_clicked);
    ui->menu_F->addAction(actExport);

    ui->menu_F->addSeparator();

    auto *actQuit = new QAction(QStringLiteral("退出"), this);
    actQuit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(actQuit, &QAction::triggered, this, &QMainWindow::close);
    ui->menu_F->addAction(actQuit);

    //=== 编辑(&E) ===
    auto *actAddRow = new QAction(QStringLiteral("添加行"), this);
    actAddRow->setShortcut(QKeySequence(Qt::Key_Insert));
    connect(actAddRow, &QAction::triggered, this, &MainWindow::on_PlusLine_Btn_clicked);
    ui->menu_E->addAction(actAddRow);

    auto *actDeleteRow = new QAction(QStringLiteral("删除行"), this);
    actDeleteRow->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(actDeleteRow, &QAction::triggered, this, &MainWindow::on_DeleteLine_Btn_clicked);
    ui->menu_E->addAction(actDeleteRow);

    auto *actClearRow = new QAction(QStringLiteral("清空行内容"), this);
    actClearRow->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Delete")));
    connect(actClearRow, &QAction::triggered, this, &MainWindow::onClearRowContent);
    ui->menu_E->addAction(actClearRow);

    ui->menu_E->addSeparator();

    auto *actSelectAll = new QAction(QStringLiteral("全选"), this);
    actSelectAll->setShortcut(QKeySequence(QStringLiteral("Ctrl+A")));
    connect(actSelectAll, &QAction::triggered, this, &MainWindow::onSelectAll);
    ui->menu_E->addAction(actSelectAll);

    auto *actClearSel = new QAction(QStringLiteral("取消选择"), this);
    actClearSel->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(actClearSel, &QAction::triggered, this, &MainWindow::onClearSelection);
    ui->menu_E->addAction(actClearSel);

    //=== 视图(&V) ===
    auto *actColumnSetting = new QAction(QStringLiteral("列设置..."), this);
    actColumnSetting->setStatusTip(QStringLiteral("打开列头设置(可选列显隐)"));
    connect(actColumnSetting, &QAction::triggered, this, &MainWindow::onColumnSetting);
    ui->menu_V->addAction(actColumnSetting);

    ui->menu_V->addSeparator();

    auto *actRefresh = new QAction(QStringLiteral("刷新表格"), this);
    actRefresh->setShortcut(QKeySequence(Qt::Key_F5));
    connect(actRefresh, &QAction::triggered, this, &MainWindow::onRefreshTable);
    ui->menu_V->addAction(actRefresh);

    auto *actResetWidth = new QAction(QStringLiteral("重置列宽"), this);
    connect(actResetWidth, &QAction::triggered, this, &MainWindow::onResetColumnWidth);
    ui->menu_V->addAction(actResetWidth);

    //=== 工具(&T) ===
    auto *actSetting = new QAction(QStringLiteral("设置..."), this);
    connect(actSetting, &QAction::triggered, this, &MainWindow::on_Setting_Btn_clicked);
    ui->menu_T->addAction(actSetting);

    auto *actFFmpegCheck = new QAction(QStringLiteral("FFmpeg 环境检测"), this);
    connect(actFFmpegCheck, &QAction::triggered, this, &MainWindow::onFFmpegCheck);
    ui->menu_T->addAction(actFFmpegCheck);

    ui->menu_T->addSeparator();

    auto *actCleanExpired = new QAction(QStringLiteral("清理过期缓存"), this);
    connect(actCleanExpired, &QAction::triggered, this, &MainWindow::onCleanExpiredCache);
    ui->menu_T->addAction(actCleanExpired);

    auto *actCleanAll = new QAction(QStringLiteral("清理全部缓存"), this);
    connect(actCleanAll, &QAction::triggered, this, &MainWindow::onCleanAllCache);
    ui->menu_T->addAction(actCleanAll);

    ui->menu_T->addSeparator();

    auto *actOpenOutputDir = new QAction(QStringLiteral("打开导出目录"), this);
    connect(actOpenOutputDir, &QAction::triggered, this, &MainWindow::onOpenOutputDir);
    ui->menu_T->addAction(actOpenOutputDir);

    //=== 帮助(&H) ===
    auto *actAbout = new QAction(QStringLiteral("关于 Memoria V2.0.0"), this);
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);
    ui->menu_H->addAction(actAbout);
}

//输出按钮：打开输出设置对话框，确定后构建混流任务并启动队列
void MainWindow::on_OutputBtn_clicked()
{
    //收集当前选中的行索引(供"导出选中项"使用)
    QList<int> selectedRows;
    if (ui->MetadataTable->selectionModel()) {
        const QModelIndexList selected = ui->MetadataTable->selectionModel()->selectedRows();
        for (const QModelIndex &idx : selected)
            selectedRows.append(idx.row());
    }

    Output_Setting_Dlog dialog(selectedRows, m_dataModel->rowCount(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    //校验导出目录
    const QString outputDir = ui->OutputPath_Edit->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先设置导出路径"));
        return;
    }

    //从对话框获取导出配置
    const QList<int> targetRows = dialog.targetRowIndices();
    const OutputFormat format = dialog.selectedFormat();
    const TranscodeParams params = dialog.transcodeParams();

    //格式扩展名
    QString ext;
    switch (format) {
    case OutputFormat::MP4:  ext = "mp4";  break;
    case OutputFormat::MKV:  ext = "mkv";  break;
    case OutputFormat::MOV:  ext = "mov";  break;
    case OutputFormat::WEBM: ext = "webm"; break;
    }

    //逐行构建MuxRequest(跳过无效行)
    QList<MuxRequest> requests;
    QList<int> requestRowIndices;
    for (int row : targetRows) {
        const ParsedCacheData &data = m_dataModel->getRowData(row);
        if (!data.videoInfo.isValid())
            continue;

        MuxRequest req;
        req.videoPath = data.videoInfo.videoFilePath;
        req.audioPath = data.videoInfo.audioFilePath;
        req.format = format;
        req.params = params;
        //输出文件路径 = 导出目录/安全标题.扩展名(清洗Windows非法字符)
        const QString safeTitle = sanitizeFileName(data.videoInfo.title);
        req.outputPath = outputDir + "/" + safeTitle + "." + ext;

        requests.append(req);
        requestRowIndices.append(row);
    }

    if (requests.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("没有可导出的有效数据行(音视频路径缺失)"));
        return;
    }

    //启动任务队列(异步执行，不阻塞UI)
    m_exportRowIndices = requestRowIndices;  //保存行索引映射，供onTaskFinished使用
    m_taskQueue->start(requests, requestRowIndices);
}


//输出路径浏览按钮：选择导出目录并填入OutputPath_Edit
//(仅选目录，文件格式由"导出设置"对话框中的格式选择决定)
void MainWindow::on_OutputPath_Btn_clicked()
{
    //以当前编辑框内容为默认定位(空则使用系统最近目录)
    const QString curPath = ui->OutputPath_Edit->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择导出目录"),
        curPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;
    ui->OutputPath_Edit->setText(dir);
}

//加行按钮：在表格末尾添加一个空行
void MainWindow::on_PlusLine_Btn_clicked()
{
    m_dataModel->addEmptyRow();
}


//删行按钮：删除当前选中的行(支持多选批量删除)
void MainWindow::on_DeleteLine_Btn_clicked()
{
    QModelIndexList selected = ui->MetadataTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    //收集要删除的行号(降序排列，从后往前删避免索引错位)
    QList<int> rows;
    rows.reserve(selected.size());
    for (const QModelIndex &idx : selected) {
        rows.append(idx.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    //逐行删除(从后往前)
    for (int row : rows) {
        deleteCacheForRow(row);
        m_dataModel->removeRow(row);
    }
}

//独立导入按钮：打开独立音视频导入对话框，确认后追加一行到表格
void MainWindow::on_IndepImport_Btn_clicked()
{
    Independ_Import_Dialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        //构建数据行并追加到表格末尾
        ParsedCacheData data;
        data.videoInfo.audioFilePath = dialog.audioPath();
        data.videoInfo.videoFilePath = dialog.videoPath();
        //标题为空时使用默认标题(视频哈希前8位_音频哈希前8位_日期)
        QString title = dialog.title();
        if (title.isEmpty()) {
            QString videoHash = fileHashPrefix(data.videoInfo.videoFilePath, 8);
            QString audioHash = fileHashPrefix(data.videoInfo.audioFilePath, 8);
            title = QStringLiteral("%1_%2_%3")
                .arg(videoHash)
                .arg(audioHash)
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));
        }
        data.videoInfo.title = title;
        m_dataModel->setRowData(-1, data);  //-1=追加到末尾
    }
}

//设置按钮：打开设置对话框
void MainWindow::on_Setting_Btn_clicked()
{
    Setting_Dialog dialog(m_dataModel, this);
    dialog.exec();
}

//导入按钮组
//外部设备导入按钮：打开ADB设备导入窗口(复用同一实例，支持USB/WiFi两种连接)
void MainWindow::on_ExterDevice_Input_Btn_clicked()
{
    if (!m_exterDeviceInputWindow) {
        m_exterDeviceInputWindow = new ExterDevice_Input_Weight(this);
        m_exterDeviceInputWindow->setWindowFlag(Qt::Window);
        //确认导入：将解析好的数据写入DataModel表格
        connect(m_exterDeviceInputWindow, &ExterDevice_Input_Weight::importConfirmed,
                this, &MainWindow::onImportConfirmed);
    }
    m_exterDeviceInputWindow->show();
    m_exterDeviceInputWindow->raise();
    m_exterDeviceInputWindow->activateWindow();
}


//本地文件导入按钮：批量选择缓存离线诊断ID文件夹，解析后导入表格
void MainWindow::on_LocalCache_Btn_clicked()
{
    //使用非原生对话框以支持多目录选择(Qt原生限制)
    QFileDialog dialog(this, QStringLiteral("选择缓存离线诊断ID文件夹(可多选)"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    //启用多选：QFileDialog内部可能用QListView(图标/列表模式)或QTreeView(详细模式)
    //两者都查找并设为扩展选择模式(支持Shift连续多选、Ctrl不连续多选)
    QListView *listView = dialog.findChild<QListView*>("listView");
    if (listView)
        listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QTreeView *treeView = dialog.findChild<QTreeView*>("treeView");
    if (treeView)
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedDirs = dialog.selectedFiles();
    if (selectedDirs.isEmpty())
        return;

    //逐个解析选中的文件夹，收集所有ParsedCacheData
    CacheFileParser parser;
    int totalImported = 0;
    int failCount = 0;

    for (const QString &dir : selectedDirs) {
        QList<ParsedCacheData> dataList;
        if (parser.Cathe_Parse(dir, dataList)) {
            for (const ParsedCacheData &data : dataList) {
                m_dataModel->setRowData(-1, data);  //-1=追加到末尾
                totalImported++;
            }
        } else {
            failCount++;
        }
    }

    //提示导入结果
    if (totalImported == 0) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("未能从选中的文件夹中解析出有效数据"));
    } else if (failCount > 0) {
        QMessageBox::information(this, QStringLiteral("导入完成"),
            QStringLiteral("成功导入 %1 条数据，%2 个文件夹解析失败").arg(totalImported).arg(failCount));
    }
}

//外部设备导入确认：接收ExterDevice_Input_Weight传来的解析数据，逐条写入DataModel
void MainWindow::onImportConfirmed(const QList<ParsedCacheData> &dataList)
{
    int count = 0;
    for (const ParsedCacheData &data : dataList) {
        m_dataModel->setRowData(-1, data);  //-1=追加到末尾
        count++;
    }

    if (count > 0) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
            QStringLiteral("已从外部设备导入 %1 条数据").arg(count));
    }
}

// ============================================================
// 缓存生命周期管理
// ============================================================

//删除指定行对应的ADB缓存（若该行数据来源于ADB导入）
void MainWindow::deleteCacheForRow(int row)
{
    if (row < 0 || row >= m_dataModel->rowCount())
        return;

    const ParsedCacheData &data = m_dataModel->getRowData(row);
    QString cachePath = data.videoInfo.cacheRootPath;

    //仅当路径位于缓存目录内时才删除（本地导入的文件不删）
    auto &cm = CacheManager::instance();
    if (cm.isInCacheDir(cachePath)) {
        QString folderName = cm.folderNameFromPath(cachePath);
        if (!folderName.isEmpty()) {
            cm.deleteFolder(folderName);
        }
    }
}

//单个导出任务完成：成功则删除对应缓存（混流已产出，缓存不再需要）
void MainWindow::onTaskFinished(int taskIndex, bool success, const QString &message)
{
    Q_UNUSED(message)
    if (!success)
        return;

    //通过成员变量映射 taskIndex → 表格行索引
    if (taskIndex < 0 || taskIndex >= m_exportRowIndices.size())
        return;

    int row = m_exportRowIndices[taskIndex];
    deleteCacheForRow(row);
}

//总进度条：单个任务开始，更新进度条到该任务的起始份额
void MainWindow::onTaskStarted(int taskIndex, int totalTasks)
{
    m_totalTasks = totalTasks;
    ui->Sum_progrBar->setRange(0, 100);
    ui->Sum_progrBar->setValue(taskIndex * 100 / totalTasks);
}

//总进度条：单个任务进度更新，按任务占比折算为整体百分比
void MainWindow::onTaskProgress(int taskIndex, int percent)
{
    if (m_totalTasks <= 0)
        return;
    int total = (taskIndex * 100 + percent) / m_totalTasks;
    ui->Sum_progrBar->setValue(qMin(total, 100));
}

//全部任务完成：进度条置满 + 弹出完成通知
void MainWindow::onAllFinished(int successCount, int failCount)
{
    //空批次(队列为空直接返回)不弹窗、不动进度条
    if (successCount == 0 && failCount == 0)
        return;

    ui->Sum_progrBar->setValue(100);

    QString msg;
    if (failCount == 0) {
        msg = QStringLiteral("全部 %1 个任务导出成功。").arg(successCount);
    } else {
        msg = QStringLiteral("导出完成：成功 %1 个，失败 %2 个。\n失败的任务已恢复为待导出状态，可重试。")
                  .arg(successCount).arg(failCount);
    }
    QMessageBox::information(this, QStringLiteral("导出完成"), msg);
}

//=== 菜单栏-编辑 ===

//清空选中行内容(重置为默认空行，保留行结构)
void MainWindow::onClearRowContent()
{
    if (!ui->MetadataTable->selectionModel())
        return;
    const QModelIndexList selected = ui->MetadataTable->selectionModel()->selectedRows();
    for (const QModelIndex &idx : selected)
        m_dataModel->setRowData(idx.row(), ParsedCacheData());
}

//全选表格所有行
void MainWindow::onSelectAll()
{
    ui->MetadataTable->selectAll();
}

//取消选择
void MainWindow::onClearSelection()
{
    ui->MetadataTable->clearSelection();
}

//=== 菜单栏-视图 ===

//列设置：打开设置对话框并强制跳转到列头设置tab
void MainWindow::onColumnSetting()
{
    Setting_Dialog dialog(m_dataModel, this);
    dialog.setCurrentTab(0);   //0=列头设置
    dialog.exec();
}

//刷新表格(通知视图重新查询模型数据)
void MainWindow::onRefreshTable()
{
    m_dataModel->layoutChanged();
}

//重置列宽为默认值(120px)
void MainWindow::onResetColumnWidth()
{
    QHeaderView *header = ui->MetadataTable->horizontalHeader();
    for (int i = 0; i < header->count(); ++i)
        header->resizeSection(i, 120);
}

//=== 菜单栏-工具 ===

//FFmpeg环境检测：重新自检并更新状态标签
void MainWindow::onFFmpegCheck()
{
    const QString ffmpegPath = m_taskQueue->selfCheckFFmpeg();
    if (ffmpegPath.isEmpty()) {
        ui->FFmpegEnvLabel->setText(QStringLiteral("❌ 未找到 FFmpeg 环境"));
        ui->FFmpegEnvLabel->setToolTip(QStringLiteral(
            "未检测到 FFmpeg。请将 ffmpeg.exe 加入系统 PATH，或放入软件目录下的 FFmpeg_tools/bin/"));
        QMessageBox::warning(this, QStringLiteral("FFmpeg 环境检测"),
            QStringLiteral("未检测到 FFmpeg 环境。\n请将 ffmpeg.exe 加入系统 PATH，"
                           "或放入软件目录下的 FFmpeg_tools/bin/"));
    } else {
        ui->FFmpegEnvLabel->setText(QStringLiteral("✅ FFmpeg: %1").arg(ffmpegPath));
        ui->FFmpegEnvLabel->setToolTip(QStringLiteral("FFmpeg 路径: %1").arg(ffmpegPath));
        QMessageBox::information(this, QStringLiteral("FFmpeg 环境检测"),
            QStringLiteral("已检测到 FFmpeg：\n%1").arg(ffmpegPath));
    }
}

//清理过期缓存
void MainWindow::onCleanExpiredCache()
{
    auto &cm = CacheManager::instance();
    qint64 beforeSize = cm.cacheSize();
    cm.cleanExpired(cm.expiryDays());
    qint64 afterSize = cm.cacheSize();
    QMessageBox::information(this, QStringLiteral("清理过期缓存"),
        QStringLiteral("已释放 %1 缓存空间。").arg(CacheManager::formatSize(beforeSize - afterSize)));
}

//清理全部缓存(二次确认)
void MainWindow::onCleanAllCache()
{
    auto &cm = CacheManager::instance();
    qint64 beforeSize = cm.cacheSize();
    if (beforeSize == 0) {
        QMessageBox::information(this, QStringLiteral("缓存清理"),
            QStringLiteral("缓存为空，无需清理。"));
        return;
    }
    auto ret = QMessageBox::warning(this, QStringLiteral("确认清理全部缓存"),
        QStringLiteral("将清理全部缓存（%1），此操作不可撤销。\n确定继续？")
            .arg(CacheManager::formatSize(beforeSize)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;
    cm.cleanAll();
    QMessageBox::information(this, QStringLiteral("清理完成"),
        QStringLiteral("已释放 %1 缓存空间。").arg(CacheManager::formatSize(beforeSize)));
}

//打开导出目录(在系统文件管理器中打开)
void MainWindow::onOpenOutputDir()
{
    const QString outputDir = ui->OutputPath_Edit->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("尚未设置导出路径。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir));
}

//=== 菜单栏-帮助 ===

//关于对话框
void MainWindow::onAbout()
{
    QMessageBox::about(this, QStringLiteral("关于 Memoria V2.0.0"),
        QStringLiteral("<h3>Memoria V2.0.0</h3>"
                       "<p>B站缓存混流导出工具</p>"
                       "<p>基于 Qt + FFmpeg 构建</p>"));
}

//窗口关闭事件：根据设置清理缓存
void MainWindow::closeEvent(QCloseEvent *event)
{
    auto &cm = CacheManager::instance();
    if (cm.cleanOnClose()) {
        cm.cleanAll();
    } else {
        cm.cleanExpired(cm.expiryDays());
    }

    QMainWindow::closeEvent(event);
}
