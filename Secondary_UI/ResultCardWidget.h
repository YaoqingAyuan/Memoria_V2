#ifndef RESULTCARDWIDGET_H
#define RESULTCARDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include "Net_Module/BiliSearchResult.h"

//搜索结果横版卡片: 封面在上(120x68), 标题在下(最多2行)
//替代QListWidget IconMode, 用QHBoxLayout管理, 彻底避免布局缓存问题
class ResultCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ResultCardWidget(const BiliSearchResult &result, QWidget *parent = nullptr)
        : QWidget(parent), m_data(result), m_bvid(result.bvid)
    {
        setFixedSize(120, 110);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(
            "ResultCardWidget { background: #2d2d30; border-radius: 4px; }"
            "ResultCardWidget:hover { background: #3d3d40; border: 1px solid #00a1d6; }"
            "QLabel { background: transparent; }"
        );

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        m_coverLabel = new QLabel(this);
        m_coverLabel->setFixedSize(120, 68);
        m_coverLabel->setAlignment(Qt::AlignCenter);
        m_coverLabel->setStyleSheet("background: #1a1a1a; border-radius: 3px;");
        layout->addWidget(m_coverLabel);

        m_titleLabel = new QLabel(this);
        m_titleLabel->setWordWrap(true);
        m_titleLabel->setStyleSheet("color: #e0e0e0; font-size: 12px; padding: 2px 4px;");
        m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_titleLabel->setText(m_data.title);
        m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        layout->addWidget(m_titleLabel, 0, Qt::AlignLeft | Qt::AlignTop);

        if (!result.isAvailable) {
            m_titleLabel->setStyleSheet("color: #ff5555; font-size: 12px; padding: 2px 4px;");
        }
    }

    void setCover(const QPixmap &pixmap)
    {
        if (pixmap.isNull())
            return;
        QPixmap scaled = pixmap.scaled(120, 68,
            Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        m_coverLabel->setPixmap(scaled.copy(
            (scaled.width() - 120) / 2,
            (scaled.height() - 68) / 2,
            120, 68));
        m_hasCover = true;
    }

    //复用卡片时更新数据(bvid不变则保留已下载的封面)
    void updateData(const BiliSearchResult &result)
    {
        bool bvidChanged = (m_bvid != result.bvid);
        m_data = result;
        m_bvid = result.bvid;
        m_titleLabel->setText(result.title);
        if (!result.isAvailable) {
            m_titleLabel->setStyleSheet("color: #ff5555; font-size: 12px; padding: 2px 4px;");
        } else {
            m_titleLabel->setStyleSheet("color: #e0e0e0; font-size: 12px; padding: 2px 4px;");
        }
        if (bvidChanged) {
            m_coverLabel->clear();
            m_hasCover = false;
        }
    }

    bool hasCover() const { return m_hasCover; }
    QString bvid() const { return m_bvid; }
    const BiliSearchResult &data() const { return m_data; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            emit clicked();
        QWidget::mousePressEvent(event);
    }

private:
    BiliSearchResult m_data;
    QString m_bvid;
    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    bool m_hasCover = false;
};

#endif // RESULTCARDWIDGET_H
