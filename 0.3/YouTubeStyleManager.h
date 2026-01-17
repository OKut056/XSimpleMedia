#ifndef YOUTUBESTYLEMANAGER_H
#define YOUTUBESTYLEMANAGER_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFutureWatcher>
#include <QLineEdit>
#include <QPair>
#include <QIcon>
#include <QMap>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

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
    void filterContent(const QString &text); // 筛选内容的槽函数
    void updateVideoTags(const QString &path,
                         const QStringList &tags);

private:
    void applyStyle();
    void loadTags();
    void saveTags() const;
    QString tagFilePath() const;

    QStackedWidget *mainStack;
    QWidget *browserPage;
    VideoDetailWidget *detailPage;

    QListWidget *contentGrid;
    QCheckBox *checkImages;
    QCheckBox *checkVideos;
    QLabel *pathLabel;

    QString currentPath;

    QLineEdit *searchEdit; // 搜索框指针

    // 异步缩略图加载器
    QFutureWatcher<QPair<int, QIcon>> *iconWatcher;

    QMap<QString, QStringList> videoTags;   // 路径 -> 标签
};

#endif // YOUTUBESTYLEMANAGER_H
