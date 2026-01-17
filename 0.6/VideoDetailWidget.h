#ifndef VIDEODETAILWIDGET_H
#define VIDEODETAILWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QList>
#include <QFutureWatcher>
#include <QStringList>
#include <QThreadPool>

// 前置声明：防止编译时报 "unknown type name"
class QVBoxLayout;
class QHBoxLayout;
class QLineEdit;
class QPushButton;

class VideoDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoDetailWidget(QWidget *parent = nullptr);

    void setVideoPath(const QString &path);
    void setTags(const QStringList &tags);

signals:
    void backRequested();
    void tagsChanged(const QString &path, const QStringList &tags);

private slots:
    void playCurrentVideo();
    void handleTagInputFinished(); // 处理输入完成

private:
    void generateScreenshots();
    void executeFFmpeg(const QString &input, int seconds, const QString &output);
    void showTagInput(QPushButton *addBtn); // 辅助函数

private:
    QLabel *coverLabel;
    QLabel *titleLabel;
    QLabel *infoLabel;
    QList<QLabel*> screenshotLabels;
    QString currentVideoPath;
    QFutureWatcher<void> shotWatcher;

    QStringList m_tags;
    QHBoxLayout *tagLayout;

    // 追踪当前的输入框
    QLineEdit *m_tagInput = nullptr;
    QThreadPool *detailThreadPool = nullptr;  // 详情页专用线程池
};

#endif // VIDEODETAILWIDGET_H
