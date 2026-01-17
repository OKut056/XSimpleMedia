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
#include <QVector>
#include <QSet>
#include <QQueue>
#include <QEvent>
#include <QTimer>

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
    void handleVideoRenamed(const QString &oldPath, const QString &newPath);

private:
    void applyStyle();
    void loadTags();
    void saveTags() const;
    void rebuildFolderList();   // 重新构建左侧子文件夹列表
    void handleFolderCheckBoxToggled(bool checked); // 子文件夹勾选变化
    void updateBackButtonState();   // 更新返回按钮显隐
    void scheduleVisibleThumbnails();  // 根据视口把“附近条目”加入任务队列
    void tryStartNextThumbBatch();     // 从队列取下一批任务交给 QtConcurrent 跑
    void onContentViewportChanged();
    void updateVisibleThumbnails();     // 根据当前视口调度缩略图

    QString tagFilePath() const;
    QTimer *scrollDebounceTimer;

    // === 缩略图任务结构 + 待处理列表 ===
    struct LoadTask {
        int index;
        QString path;
        bool isVideo;
    };
    QVector<LoadTask> pendingThumbTasks;
    // === 启动缩略图生成的内部函数 ===
    void startThumbnailGeneration();

    // 等待生成缩略图的任务队列
    QQueue<LoadTask> thumbTaskQueue;
    // 已经请求过生成（在队列或正在执行）的条目索引
    QSet<int> thumbRequested;
    // 已经成功生成缩略图的条目索引
    QSet<int> thumbReady;
    static const int ThumbBatchSize = 32; // 一次最多处理多少个缩略图

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
    // 用于跟踪视口变化（滚动 / 尺寸变化）
    bool eventFilter(QObject *obj, QEvent *event) override;

    // --- 缓存相关结构 ---
    struct DirCache {
        QList<QListWidgetItem*> items; // 保存所有的 Item 指针（带图标）
        int scrollPosition;            // 保存离开时的滚动条位置
        QSet<int> thumbReady;          // 保存哪些缩略图已经加载好了
    };

    // 路径 -> 缓存数据
    QMap<QString, DirCache> m_dirCache;

    // 记录上一次显示的路径，用于判断是“离开”还是“刷新”
    QString m_lastLoadedPath;

    // 辅助函数：清空所有缓存（防止内存泄漏）
    void clearAllCache();

    static bool isVideoSuffix(const QString &suffix) {
        return (suffix == "mp4" || suffix == "mkv" || suffix == "avi" || suffix == "mov" ||
                suffix == "webm" || suffix == "flv" || suffix == "wmv" || suffix == "m4v");
    }
};

#endif // YOUTUBESTYLEMANAGER_H
