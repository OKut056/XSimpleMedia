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
#include <QFileSystemWatcher>

// 前置声明
class VideoDetailWidget;
class QVBoxLayout;
class QWidget;
class QCheckBox;

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
    void updateVideoTags(const QString &path, const QStringList &tags);
    void goUpDirectory();   // 返回上一级目录
    void onDirectoryChanged(const QString &path);   // 监控目录变化

private:
    void applyStyle();
    void loadTags();
    void saveTags() const;
    void rebuildFolderList();   // 重新构建左侧子文件夹列表
    void handleFolderCheckBoxToggled(bool checked); // 子文件夹勾选变化
    void updateBackButtonState();   // 更新返回按钮显隐

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
    QFutureWatcher<QPair<int, QIcon>> *iconWatcher; // 异步缩略图加载器
    QMap<QString, QStringList> videoTags;   // 路径 -> 标签
    QWidget *folderListContainer = nullptr;   // 底部区域容器
    QVBoxLayout *folderListLayout = nullptr;  // 子文件夹复选框列表布局
    QPushButton *backButton = nullptr; // 返回按钮
    QFileSystemWatcher *dirWatcher = nullptr;   // 目录监视器

    bool updatingFolderChecks = false;        // 防止递归更新
};

#endif // YOUTUBESTYLEMANAGER_H
