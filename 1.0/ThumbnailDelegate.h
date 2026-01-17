#ifndef THUMBNAILDELEGATE_H
#define THUMBNAILDELEGATE_H

#include <QStyledItemDelegate>

class ThumbnailDelegate : public QStyledItemDelegate {
public:
    explicit ThumbnailDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // THUMBNAILDELEGATE_H
