// TagButton.h
#ifndef TAGBUTTON_H
#define TAGBUTTON_H

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

class TagButton : public QPushButton {
    Q_OBJECT
public:
    explicit TagButton(const QString &tagText, QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_text(tagText)
    {
        setFixedHeight(28);
        setCursor(Qt::ArrowCursor);

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 0, 6, 0);
        layout->setSpacing(8);

        QLabel *textLabel = new QLabel(tagText, this);
        textLabel->setStyleSheet("color: #f1f1f1; font-size: 13px;");
        layout->addWidget(textLabel);

        QPushButton *delBtn = new QPushButton("x", this);
        delBtn->setFixedSize(16, 16);
        delBtn->setStyleSheet(
            "QPushButton {"
            "   background: transparent; color: #aaa; font-size: 12px; border-radius: 8px;"
            "}"
            "QPushButton:hover {"
            "   background: #444; color: white;"
            "}"
            );
        connect(delBtn, &QPushButton::clicked, this, [this]() {
            emit deleted(m_text);
        });
        layout->addWidget(delBtn);

        setStyleSheet(
            "QPushButton {"
            "   background-color: #1e1e1e; border: 1px solid #333; border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2a2a2a; border-color: #444;"
            "}"
            );
    }

    QString tagText() const { return m_text; }

signals:
    void deleted(const QString &text);

private:
    QString m_text;
};

#endif // TAGBUTTON_H
