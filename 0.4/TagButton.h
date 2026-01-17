// TagButton.h
#ifndef TAGBUTTON_H
#define TAGBUTTON_H

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QStyle>

class TagButton : public QPushButton {
    Q_OBJECT
public:
    explicit TagButton(const QString &tagText, QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_text(tagText)
    {
        // 1. 设置对象名，用于隔离样式表，防止样式污染子控件
        setObjectName("TagButtonRoot");

        // 2. 只有高度固定，宽度交由布局管理器自动计算
        setFixedHeight(28);

        // 3. 设置大小策略：水平方向根据内容变化
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        setCursor(Qt::ArrowCursor);

        QHBoxLayout *layout = new QHBoxLayout(this);
        // 设置边距：左侧留多点给文字，右侧紧凑一点
        layout->setContentsMargins(6, 0, 4, 0);
        layout->setSpacing(3);

        // 4. 【关键】强制 Widget 的大小严格跟随布局的内容大小
        // 这样文字多长，按钮就自动多宽
        layout->setSizeConstraint(QLayout::SetFixedSize);

        // 标签文字
        QLabel *textLabel = new QLabel(tagText, this);
        textLabel->setStyleSheet("color: #f1f1f1; font-size: 13px; border: none; background: transparent;");
        textLabel->setAttribute(Qt::WA_TransparentForMouseEvents); // 让点击能够穿透 Label到达 Button
        layout->addWidget(textLabel);

        // 删除按钮
        QPushButton *delBtn = new QPushButton("x", this);
        delBtn->setObjectName("TagDelBtn"); // 设置ID
        delBtn->setFixedSize(16, 16);
        delBtn->setCursor(Qt::PointingHandCursor);

        // 删除按钮单独的样式 (使用ID选择器 #TagDelBtn)
        // 避免继承父类的边框和背景
        delBtn->setStyleSheet(
            "#TagDelBtn {"
            "   background: transparent; color: #aaa; font-size: 12px; border-radius: 8px; border: none;"
            "   padding-bottom: 2px;" /* 微调 x 的垂直位置 */
            "}"
            "#TagDelBtn:hover {"
            "   background: #444; color: white;"
            "}"
            );

        connect(delBtn, &QPushButton::clicked, this, [this]() {
            emit deleted(m_text);
        });
        layout->addWidget(delBtn);

        // 主按钮样式 (使用ID选择器 #TagButtonRoot)
        // 这样不会影响内部的 delBtn
        setStyleSheet(
            "#TagButtonRoot {"
            "   background-color: #1e1e1e; border: 1px solid #333; border-radius: 4px;"
            "   text-align: left;" /* 确保文字靠左 */
            "}"
            "#TagButtonRoot:hover {"
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
