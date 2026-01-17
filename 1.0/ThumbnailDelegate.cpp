#include "ThumbnailDelegate.h"

#include <QPainter>
#include <QIcon>
#include <QFont>
#include <QFontMetrics>

ThumbnailDelegate::ThumbnailDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void ThumbnailDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 数据
    QString text = index.data(Qt::DisplayRole).toString();
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

    QRect rect = option.rect;
    rect.adjust(6, 6, -6, -6); // 边距

    // 背景 (选中/悬停)
    QColor bgColor = Qt::transparent;
    if (option.state & QStyle::State_Selected) {
        bgColor = QColor("#3ea6ff");
    } else if (option.state & QStyle::State_MouseOver) {
        bgColor = QColor("#2d2d2d");
    }

    if (bgColor != Qt::transparent) {
        painter->setBrush(bgColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rect, 8, 8);
    }

    // 预留底部区域给文字
    QRect contentRect(rect.left() + 8, rect.top() + 8,
                      rect.width() - 16, rect.height() - 40);

    // 缩略图
    if (!icon.isNull()) {
        QPixmap pix = icon.pixmap(contentRect.size());

        // 图片在容器中的位置
        int x = contentRect.left() +
                (contentRect.width() - pix.width()) / 2 + 18;
        int y = contentRect.top() +
                (contentRect.height() - pix.height()) / 2 + 5;

        painter->drawPixmap(x, y, pix);

        // 如果是视频，绘制播放三角形
        bool isVideo = index.data(Qt::UserRole + 1).toBool();
        if (isVideo) {
            painter->setBrush(QColor(0, 0, 0, 150));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(contentRect.center(), 20, 20);

            painter->setBrush(Qt::white);
            QPoint center = contentRect.center();
            QPolygon tri;
            tri << QPoint(center.x() - 6, center.y() - 8)
                << QPoint(center.x() - 6, center.y() + 8)
                << QPoint(center.x() + 8, center.y());
            painter->drawPolygon(tri);
        }
    }

    // 文字，超出用省略号
    QRect textRect(rect.left() + 5,
                   rect.bottom() - 30,
                   rect.width() - 10,
                   25);
    painter->setPen((option.state & QStyle::State_Selected)
                        ? Qt::black
                        : QColor("#e0e0e0"));
    painter->setFont(QFont("Segoe UI", 9));

    QString elidedText =
        painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignCenter, elidedText);

    painter->restore();
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem &,
                                  const QModelIndex &) const {
    return QSize(180, 160);
}
