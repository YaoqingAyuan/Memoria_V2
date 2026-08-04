#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "DataModel.h"
#include "ParsedCacheData.h"
#include "Secondary_UI/Setting_Dialog.h"
#include "Secondary_UI/Independ_Import_Dialog.h"
#include <QHeaderView>
#include <QContextMenuEvent>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dataModel(new DataModel(this))
{
    ui->setupUi(this);

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
                //TODO: 从对话框获取路径数据，填充到m_dataModel的对应行
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

//输出按钮(路径设置、导出)
void MainWindow::on_OutputBtn_clicked()
{

}


void MainWindow::on_OutputPath_Btn_clicked()
{

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

//独立导入按钮
void MainWindow::on_IndepImport_Btn_clicked()
{

}

//设置按钮：打开设置对话框
void MainWindow::on_Setting_Btn_clicked()
{
    Setting_Dialog dialog(m_dataModel, this);
    dialog.exec();
}

//导入按钮组
void MainWindow::on_Link_Input_Btn_clicked()
{

}


void MainWindow::on_WLAN_Input_Btn_clicked()
{

}


void MainWindow::on_LocalCache_Btn_clicked()
{

}
