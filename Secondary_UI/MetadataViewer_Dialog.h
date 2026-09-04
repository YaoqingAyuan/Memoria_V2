#ifndef METADATAVIEWER_DIALOG_H
#define METADATAVIEWER_DIALOG_H
//元数据查看对话框：右键表格行 → 查看原始元数据
//展示该行 ParsedCacheData 中的 entryJsonData + indexJsonData 全部键值对

#include <QDialog>
#include <QTreeWidget>
#include "core/ParsedCacheData.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MetadataViewer_Dialog;
}
QT_END_NAMESPACE

class MetadataViewer_Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetadataViewer_Dialog(const ParsedCacheData &data, QWidget *parent = nullptr);
    ~MetadataViewer_Dialog();

private:
    Ui::MetadataViewer_Dialog *ui;

    //将 MetadataContainer 中的键值对填充到指定的 QTreeWidget
    void fillTree(QTreeWidget *tree, const MetadataContainer &container);
};

#endif // METADATAVIEWER_DIALOG_H
