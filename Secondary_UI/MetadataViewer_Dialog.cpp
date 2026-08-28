#include "MetadataViewer_Dialog.h"
#include "ui_MetadataViewer_Dialog.h"

MetadataViewer_Dialog::MetadataViewer_Dialog(const ParsedCacheData &data, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MetadataViewer_Dialog)
{
    ui->setupUi(this);

    //标题栏显示视频标题
    setWindowTitle(QStringLiteral("原始元数据 — %1").arg(data.videoInfo.title));

    //填充 entry.json 数据
    fillTree(ui->entryTree, data.entryJsonData);
    ui->entryCountLabel->setText(QStringLiteral("共 %1 项").arg(data.entryJsonData.size()));

    //填充 index.json 数据
    fillTree(ui->indexTree, data.indexJsonData);
    ui->indexCountLabel->setText(QStringLiteral("共 %1 项").arg(data.indexJsonData.size()));
}

MetadataViewer_Dialog::~MetadataViewer_Dialog()
{
    delete ui;
}

void MetadataViewer_Dialog::fillTree(QTreeWidget *tree, const MetadataContainer &container)
{
    tree->clear();
    for (auto it = container.constBegin(); it != container.constEnd(); ++it) {
        QTreeWidgetItem *item = new QTreeWidgetItem(tree);
        item->setText(0, it.key());
        item->setText(1, it.value());
    }
    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
}
