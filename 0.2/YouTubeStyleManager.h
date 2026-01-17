#ifndef YOUTUBESTYLEMANAGER_H
#define YOUTUBESTYLEMANAGER_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QMap>
#include <QLineEdit>

// 前置声明
class VideoDetailWidget;

class YouTubeStyleManager : public QMainWindow {
    Q_OBJECT

public:
    explicit YouTubeStyleManager(QWidget *parent = nullptr);
    ~YouTubeStyleManager();

public slots:
    void loadContent();
    void handleItemClicked(QListWidgetItem *item);
    void openFolder();
    void showBrowser();

private slots:
    void onThumbnailLoaded(int index);
    void filterContent(const QString &text);// 新增：筛选内容的槽函数

private:
    void applyStyle();

    QStackedWidget *mainStack;
    QWidget *browserPage;
    VideoDetailWidget *detailPage;

    QListWidget *contentGrid;
    QCheckBox *checkImages;
    QCheckBox *checkVideos;
    QLabel *pathLabel;

    QString currentPath;

    QLineEdit *searchEdit; // 新增：搜索框指针

    // 异步缩略图加载器
    QFutureWatcher<QPair<int, QIcon>> *iconWatcher;
};

#endif // YOUTUBESTYLEMANAGER_H
