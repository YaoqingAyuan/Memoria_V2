#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "Core/DataModel.h"
#include "Core/ParsedCacheData.h"
#include "TaskQueue/TaskQueue.h"
#include "FFmpeg_Module/FFmpeg_module.h"
#include "Parser_Module/CacheFileParser.h"
#include "Core/utils.h"
#include "Secondary_UI/Setting_Dialog.h"
#include "Secondary_UI/Independ_Import_Dialog.h"
#include "Secondary_UI/Output_Setting_Dlog.h"
#include "Secondary_UI/Link_Input_Weight.h"
#include "Secondary_UI/WLAN_Input_Weight.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QListView>
#include <QTreeView>
#include <QDateTime>
#include <QMessageBox>
#include <algorithm>
#include "Core/utils.h"

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
                    m_dataModel->removeRow(r);
                }
            } else {
                //单行删除
                m_dataModel->removeRow(index.row());
            }
        } else if (selected == clearAction) {
            //重置该行为默认空行
            m_dataModel->setRowData(index.row(), ParsedCacheData());
        }
    });
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
//外部有线导入按钮：打开有线导入窗口(复用同一实例)
void MainWindow::on_Link_Input_Btn_clicked()
{
    if (!m_linkInputWindow) {
        m_linkInputWindow = new Link_Input_Weight(this);
        //以独立顶层窗口形式显示(而非嵌入主窗口)
        m_linkInputWindow->setWindowFlag(Qt::Window);
    }
    m_linkInputWindow->show();
    m_linkInputWindow->raise();
    m_linkInputWindow->activateWindow();
}


//外部无线导入按钮：打开无线导入窗口(复用同一实例)
void MainWindow::on_WLAN_Input_Btn_clicked()
{
    if (!m_wlanInputWindow) {
        m_wlanInputWindow = new WLAN_Input_Weight(this);
        m_wlanInputWindow->setWindowFlag(Qt::Window);
    }
    m_wlanInputWindow->show();
    m_wlanInputWindow->raise();
    m_wlanInputWindow->activateWindow();
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
