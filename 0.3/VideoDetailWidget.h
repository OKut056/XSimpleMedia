#ifndef VIDEODETAILWIDGET_H
#define VIDEODETAILWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QList>
#include <QFutureWatcher>
#include <QLineEdit>
#include <QStringList>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

class VideoDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoDetailWidget(QWidget *parent = nullptr);

    void setVideoPath(const QString &path);
    void setTags(const QStringList &tags);   // 外部设置标签

signals:
    void backRequested();
    void tagsChanged(const QString &path, const QStringList &tags);

private slots:
    void playCurrentVideo();

private:
    void generateScreenshots();
    void executeFFmpeg(const QString &input, int seconds, const QString &output);
    void initTagUi();                        // 初始化标签区域
    void addTagButton(const QString &tagText); // 在布局中添加一个 TagButton

private:
    QLabel *coverLabel;
    QLabel *titleLabel;
    QLabel *infoLabel;
    QList<QLabel*> screenshotLabels;
    QString currentVideoPath;
    QFutureWatcher<void> shotWatcher;
    QStringList m_tags;                      // 新增：当前视频的标签
    QHBoxLayout *tagLayout;                  // 新增：标签方块所在的布局
};

#endif // VIDEODETAILWIDGET_H
