#include "YouTubeStyleManager.h"
#include "ThumbnailDelegate.h"
#include "VideoDetailWidget.h"
#include "FfmpegUtil.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStyle>
#include <QDesktopServices>
#include <QUrl>
#include <QCryptographicHash>
#include <QImageReader>
#include <QProcess>
#include <QtConcurrent>
#include <QAbstractItemView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QThreadPool>
#include <QScrollBar>
#include <QEvent>
#include <QScrollArea>

YouTubeStyleManager::YouTubeStyleManager(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("XSimple Media Manager");
    resize(1280, 800);
    updatingFolderChecks = false;

    // 初始路径：视频目录，不存在则使用 home
    currentPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (currentPath.isEmpty()) currentPath = QDir::homePath();

    // 异步缩略图 watcher
    iconWatcher = new QFutureWatcher<QPair<int, QIcon>>(this);
    connect(iconWatcher, &QFutureWatcher<QPair<int, QIcon>>::resultReadyAt,
            this, &YouTubeStyleManager::onThumbnailLoaded);

    // 每批缩略图生成完成后，尝试启动下一批
    connect(iconWatcher, &QFutureWatcher<QPair<int, QIcon>>::finished,
            this, [this]() { tryStartNextThumbBatch(); });

    // === 限制全局线程池并发，防止一次性开太多 ffmpeg ===
    QThreadPool::globalInstance()->setMaxThreadCount(2);

    connect(iconWatcher, &QFutureWatcher<QPair<int, QIcon>>::finished,
            this, [this]() {
                auto future = iconWatcher->future();
                const int n = future.resultCount();
                for (int i = 0; i < n; ++i) {
                    const QPair<int, QIcon> pair = future.resultAt(i);
                    const int index = pair.first;
                    const QIcon &icon = pair.second;

                    if (!icon.isNull()) {
                        QListWidgetItem *item = contentGrid->item(index);
                        if (item)
                            item->setIcon(icon);
                    }

                    thumbReady.insert(index);
                }

                // 这个批次结束后，再看视口附近是否有新的任务需要启动
                tryStartNextThumbBatch();
            });

    mainStack = new QStackedWidget(this);
    setCentralWidget(mainStack);

    // Browser Page
    browserPage = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(browserPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧面板
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(240);

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 10, 5, 10);
    leftLayout->setSpacing(5);

    QLabel *logo = new QLabel("MediaHub", this);
    logo->setStyleSheet("color: white; font-size: 20px; font-weight: bold; "
                        "margin-bottom: 20px; padding-left: 10px;");
    leftLayout->addWidget(logo);

    checkImages = new QCheckBox("图片库", this);
    checkImages->setObjectName("sidebarItem");
    checkImages->setCursor(Qt::PointingHandCursor);
    checkImages->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    checkImages->setChecked(true);

    checkVideos = new QCheckBox("视频集", this);
    checkVideos->setObjectName("sidebarItem");
    checkVideos->setCursor(Qt::PointingHandCursor);
    checkVideos->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    checkVideos->setChecked(true);

    leftLayout->addWidget(checkImages);
    leftLayout->addWidget(checkVideos);

    QFrame *line = new QFrame(this);
    line->setFixedHeight(1);
    line->setStyleSheet("background-color: #333; margin: 15px 0;");
    leftLayout->addWidget(line);

    mainLayout->addWidget(leftPanel);

    // 右侧面板
    QWidget *rightPanel = new QWidget(this);
    rightPanel->setStyleSheet("background-color: #0f0f0f;");
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

    // 顶部工具栏布局
    QHBoxLayout *topBarLayout = new QHBoxLayout();

    pathLabel = new QLabel(currentPath, this);
    pathLabel->setStyleSheet(
        "color: #aaaaaa; font-size: 14px; font-family: 'Segoe UI'; font-weight: 500;");
    // 原代码是直接加到 rightLayout，而不是 topBar 里
    rightLayout->addWidget(pathLabel);

    // 左侧返回上一级按钮
    backButton = new QPushButton(this);
    backButton->setObjectName("backBtn");
    backButton->setFixedSize(40, 32);               // 和搜索框高度接近
    backButton->setText("<-");
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setFlat(true);                      // 去掉默认边框
    connect(backButton, &QPushButton::clicked,
            this, &YouTubeStyleManager::goUpDirectory);
    topBarLayout->addWidget(backButton);

    // 右侧搜索框
    topBarLayout->addStretch();

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("搜索...");
    searchEdit->setFixedWidth(240);
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #121212;"
        "    border: 1px solid #333333;"
        "    border-radius: 18px;"
        "    color: #ffffff;"
        "    padding: 8px 15px;"
        "    font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #3ea6ff;"
        "    background-color: #1a1a1a;"
        "}"
        );
    connect(searchEdit, &QLineEdit::textChanged,
            this, &YouTubeStyleManager::filterContent);
    topBarLayout->addWidget(searchEdit);

    topBarLayout->setContentsMargins(20, 8, 20, 5);
    rightLayout->addLayout(topBarLayout);

    // 内容网格
    contentGrid = new QListWidget(this);
    contentGrid->setViewMode(QListWidget::IconMode);
    contentGrid->setResizeMode(QListWidget::Adjust);
    contentGrid->setSpacing(12);
    contentGrid->setIconSize(QSize(180, 120));
    contentGrid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentGrid->setStyleSheet(
        "QListWidget { background-color: #0f0f0f; border: none; outline: none; }"
        "QListWidget::item { color: #f1f1f1; }");
    contentGrid->setItemDelegate(new ThumbnailDelegate(this));

    // === 统一 item 尺寸，减少布局成本 ===
    contentGrid->setUniformItemSizes(true);

    rightLayout->addWidget(contentGrid);

    mainLayout->addWidget(rightPanel);

    scrollDebounceTimer = new QTimer(this);
    scrollDebounceTimer->setSingleShot(true);
    scrollDebounceTimer->setInterval(150); // 延迟150毫秒触发
    connect(scrollDebounceTimer, &QTimer::timeout,
            this, &YouTubeStyleManager::onContentViewportChanged);

    // 视口变化时（滚动 / 尺寸改变）按需调度缩略图
    contentGrid->verticalScrollBar()->setSingleStep(24); // 可选：细腻滚动
    connect(contentGrid->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) {
                // 每次滚动都重置计时器，只有停下来才会触发 timeout
                scrollDebounceTimer->start();
            });

    // 监听视口尺寸改变
    contentGrid->viewport()->installEventFilter(this);

    // 详情页
    detailPage = new VideoDetailWidget(this);
    mainStack->addWidget(browserPage);
    mainStack->addWidget(detailPage);

    // 信号连接
    auto onFilterChanged = [this](bool) {
        // 过滤条件变了，清除当前路径的缓存，强制重新扫描
        if (m_dirCache.contains(currentPath)) {
            qDeleteAll(m_dirCache[currentPath].items);
            m_dirCache.remove(currentPath);
        }
        // 也可以选择 clearAllCache() 清除所有，防止“返回上一级”时看到旧的过滤结果
        // 建议：简单起见，清除所有缓存，保证数据一致性
        clearAllCache();
        m_lastLoadedPath.clear(); // 重置，确保 loadContent 不会尝试保存当前错误的列表

        loadContent();
    };

    connect(checkImages, &QCheckBox::toggled, this, onFilterChanged);
    connect(checkVideos, &QCheckBox::toggled, this, onFilterChanged);
    connect(contentGrid, &QListWidget::itemClicked,
            this, &YouTubeStyleManager::handleItemClicked);
    connect(detailPage, &VideoDetailWidget::backRequested,
            this, &YouTubeStyleManager::showBrowser);

    // 详情页标签变更信号
    connect(detailPage, &VideoDetailWidget::tagsChanged,
            this, &YouTubeStyleManager::updateVideoTags);

    // 底部区域：当前路径子文件夹列表 + “打开文件夹”按钮
    folderListContainer = new QWidget(this);
    QVBoxLayout *folderAreaLayout = new QVBoxLayout(folderListContainer);
    folderAreaLayout->setContentsMargins(0, 0, 0, 0);
    folderAreaLayout->setSpacing(4);

    // 子文件夹列表布局，后面由 rebuildFolderList() 填充
    folderListLayout = new QVBoxLayout();
    folderListLayout->setContentsMargins(10, 0, 5, 0);  // 左缩进，类似资源管理器
    folderListLayout->setSpacing(2);

    QWidget *folderListWidget = new QWidget(folderListContainer);
    folderListWidget->setLayout(folderListLayout);
    folderListWidget->setStyleSheet("background: transparent;"); // 确保背景透明

    // 2. [关键] 创建 ScrollArea 包裹列表控件
    QScrollArea *scrollArea = new QScrollArea(folderListContainer);
    scrollArea->setWidget(folderListWidget);
    scrollArea->setWidgetResizable(true); // 让内部控件随 ScrollArea 大小调整
    scrollArea->setFrameShape(QFrame::NoFrame); // 去掉默认边框
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用水平滚动
    scrollArea->setStyleSheet("background: transparent;"); // 确保 ScrollArea 背景透明

    // 将 ScrollArea 加入布局，而不是直接加入 folderListWidget
    folderAreaLayout->addWidget(scrollArea);

    // 底部“打开文件夹”按钮（仍然调用 openFolder，实现选择根目录）
    QPushButton *btnOpen = new QPushButton("打开文件夹", folderListContainer);
    btnOpen->setObjectName("sidebarBtn");
    btnOpen->setCursor(Qt::PointingHandCursor);
    folderAreaLayout->addWidget(btnOpen);

    // 添加到左侧面板布局时，参数 1 表示“尽可能占用剩余空间”
    // 这样 ScrollArea 才有空间展开，否则会缩成一团
    leftLayout->addWidget(folderListContainer, 1);
    connect(btnOpen, &QPushButton::clicked, this, &YouTubeStyleManager::openFolder);

    applyStyle();

    // 启动时加载已经保存的标签
    loadTags();
    loadContent();
    rebuildFolderList();   // 初次构建子文件夹列表
    updateBackButtonState();   // 根据当前路径决定是否显示返回按钮

    // 初始化目录监视器，监听当前路径
    dirWatcher = new QFileSystemWatcher(this);
    dirWatcher->addPath(currentPath);
    connect(dirWatcher, &QFileSystemWatcher::directoryChanged,
            this, &YouTubeStyleManager::onDirectoryChanged);
}

YouTubeStyleManager::~YouTubeStyleManager()
{
    // 清空等待队列，不再分发新任务
    thumbTaskQueue.clear();

    // 强制取消正在进行的异步任务，并等待后台线程安全归位
    if (iconWatcher) {
        if (iconWatcher->isRunning()) {
            iconWatcher->cancel();
            iconWatcher->waitForFinished();
        }
    }

    // 再次调用清理，确保万无一失
    // 保险：窗口销毁时尝试清理所有仍在运行的 ffmpeg 子进程
    killAllFfmpegProcesses();

    // 清理缓存中的 Item
    clearAllCache();
}

void YouTubeStyleManager::clearAllCache()
{
    for (auto &cache : m_dirCache) {
        qDeleteAll(cache.items); // 销毁所有缓存的 Item
    }
    m_dirCache.clear();
}

void YouTubeStyleManager::applyStyle() {
    QString qss = R"(
        QMainWindow, QWidget {
            background-color: #0f0f0f;
            color: #f1f1f1;
            font-family: "Segoe UI", sans-serif;
            font-size: 14px;
        }
        QWidget#leftPanel { background-color: #050505; }
        QCheckBox#sidebarItem {
            background-color: transparent;
            color: #aaaaaa;
            padding: 12px 20px;
            border-radius: 10px;
            font-size: 15px;
            margin-bottom: 2px;
            spacing: 15px;
        }
        QCheckBox::indicator#sidebarItem {
            width: 0px;
            height: 0px;
            border: none;
        }
        QCheckBox#sidebarItem:hover {
            background-color: #1f1f1f;
            color: white;
        }
        QCheckBox#sidebarItem:checked {
            background-color: #1f1f1f;
            color: white;
            font-weight: bold;
            border-left: 3px solid #3ea6ff;
        }
        QPushButton#sidebarBtn {
            background-color: transparent;
            color: #aaaaaa;
            padding: 12px 20px;
            border-radius: 10px;
            text-align: left;
            border: none;
            font-size: 15px;
        }
        QPushButton#sidebarBtn:hover {
            background-color: #1f1f1f;
            color: white;
        }
    )";

    qss += R"(
        QCheckBox#folderItem {
            background-color: transparent;
            color: #cccccc;
            padding: 6px 10px;
            border-radius: 6px;
            font-size: 13px;
            spacing: 8px;
        }
        QCheckBox::indicator#folderItem {
            width: 0px;
            height: 0px;
            border: none;
        }
        QCheckBox#folderItem:hover {
            background-color: #1f1f1f;
            color: white;
        }
        QCheckBox#folderItem:checked {
            background-color: #1f1f1f;
            color: white;
            font-weight: 500;
            border-left: 3px solid #3ea6ff;
        }
    )";

    qss += R"(
        QPushButton#backBtn {
            background-color: #202020;
            border-radius: 16px;
            border: 1px solid #3a3a3a;
            padding: 4px 12px;
            color: #ffffff;
        }
        QPushButton#backBtn:hover {
            background-color: #333333;
            border-color: #3ea6ff;
        }
        QPushButton#backBtn:pressed {
            background-color: #1a1a1a;
        }
    )";

    qss += R"(
        QScrollArea { border: none; background-color: transparent; }

        QScrollBar:vertical {
            border: none;
            background: #0f0f0f;
            width: 8px;
            margin: 0px 0px 0px 0px;
        }
        QScrollBar::handle:vertical {
            background: #444;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #666;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )";

    this->setStyleSheet(qss);
}

void YouTubeStyleManager::rebuildFolderList()
{
    if (!folderListLayout || !folderListContainer)
        return;

    // 清空旧的复选框
    while (QLayoutItem *item = folderListLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    QDir dir(currentPath);
    // 按名称排序，仿资源管理器
    QFileInfoList subdirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
        );

    for (const QFileInfo &info : subdirs) {
        QCheckBox *cb = new QCheckBox(info.fileName(), folderListContainer);
        cb->setObjectName("folderItem");
        cb->setCursor(Qt::PointingHandCursor);
        cb->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        cb->setProperty("folderPath", info.absoluteFilePath());

        connect(cb, &QCheckBox::toggled,
                this, &YouTubeStyleManager::handleFolderCheckBoxToggled);

        folderListLayout->addWidget(cb);
    }

    // 让列表从顶部开始排列
    folderListLayout->addStretch();
}

void YouTubeStyleManager::handleFolderCheckBoxToggled(bool checked)
{
    if (!checked)
        return; // 只对“勾选”事件做处理

    if (updatingFolderChecks)
        return;

    QCheckBox *cb = qobject_cast<QCheckBox *>(sender());
    if (!cb)
        return;

    QString folderPath = cb->property("folderPath").toString();
    if (folderPath.isEmpty())
        return;

    // 只允许一个子文件夹选中：仿资源管理器单选效果
    updatingFolderChecks = true;
    for (int i = 0; i < folderListLayout->count(); ++i) {
        QWidget *w = folderListLayout->itemAt(i)->widget();
        QCheckBox *other = qobject_cast<QCheckBox *>(w);
        if (other && other != cb)
            other->setChecked(false);
    }
    updatingFolderChecks = false;

    // 切换当前路径并刷新内容
    currentPath = folderPath;
    pathLabel->setText(folderPath);
    loadContent();
    rebuildFolderList();    // 切到新目录后，显示它的子目录
    updateBackButtonState(); // 更新返回按钮

    // 更新目录监视器监听的路径
    if (dirWatcher) {
        dirWatcher->removePaths(dirWatcher->directories());
        dirWatcher->addPath(currentPath);
    }
}

void YouTubeStyleManager::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);

    // 如果正在浏览的路径被修改，刷新内容区和子目录列表
    loadContent();
    rebuildFolderList();
}

void YouTubeStyleManager::updateBackButtonState()
{
    if (!backButton)
        return;

    QDir dir(currentPath);
    bool canGoUp = dir.cdUp();     // 能 cdUp 就表示有上一级

    backButton->setVisible(canGoUp);
}

void YouTubeStyleManager::goUpDirectory()
{
    QDir dir(currentPath);
    if (!dir.cdUp())
        return;    // 已经在根目录，无上一级

    currentPath = dir.absolutePath();
    pathLabel->setText(currentPath);

    loadContent();
    rebuildFolderList();
    updateBackButtonState();

    // 更新目录监视器监听的路径
    if (dirWatcher) {
        dirWatcher->removePaths(dirWatcher->directories());
        dirWatcher->addPath(currentPath);
    }
}

void YouTubeStyleManager::onThumbnailLoaded(int resultIndex) {
    if (resultIndex < 0)
        return;

    QPair<int, QIcon> result = iconWatcher->resultAt(resultIndex);
    int row = result.first;

    if (row >= 0 && row < contentGrid->count()) {
        QListWidgetItem *item = contentGrid->item(row);

        // 只有当结果有效时才更新，否则保持默认图标
        if (item && !result.second.isNull()){
            item->setIcon(result.second);
            thumbReady.insert(row); // 标记该条目缩略图已生成
        }
    }
}

void YouTubeStyleManager::loadContent()
{
    // 1. 停止当前的异步任务
    if (iconWatcher->isRunning())
        iconWatcher->cancel();

    thumbTaskQueue.clear();
    thumbRequested.clear(); // 新页面重新调度

    // ---------------------------------------------------------
    // A. [保存现场] 离开当前文件夹前，把 Item 存入缓存
    // ---------------------------------------------------------
    // 只有当路径发生变化，且旧路径有效时才缓存
    if (!m_lastLoadedPath.isEmpty() && m_lastLoadedPath != currentPath) {
        DirCache cache;
        cache.scrollPosition = contentGrid->verticalScrollBar()->value();
        cache.thumbReady = thumbReady; // 保存已加载状态

        // 关键：使用 takeItem 将 Item 从列表中“剥离”出来，而不是删除
        // 这样 Item 里的 Icon 和数据都完好无损
        const int count = contentGrid->count();
        for (int i = 0; i < count; ++i) {
            cache.items.append(contentGrid->takeItem(0));
        }

        m_dirCache.insert(m_lastLoadedPath, cache);
    }
    // 如果是路径没变（比如刷新），或者是过滤器变化导致的重载，则应该销毁旧 Item
    else {
        contentGrid->clear();
        // 并在这种情况下，通常意味着当前路径的缓存也失效了（比如切换了图片/视频过滤）
        if (m_dirCache.contains(currentPath)) {
            qDeleteAll(m_dirCache[currentPath].items);
            m_dirCache.remove(currentPath);
        }
    }

    contentGrid->setUpdatesEnabled(false);

    // ---------------------------------------------------------
    // B. [尝试恢复] 检查新路径是否有缓存
    // ---------------------------------------------------------
    if (m_dirCache.contains(currentPath)) {
        DirCache &cache = m_dirCache[currentPath];

        // 恢复所有 Item
        for (QListWidgetItem *item : cache.items) {
            contentGrid->addItem(item);
        }

        // 恢复状态
        thumbReady = cache.thumbReady;

        // 恢复滚动条位置 (需要一点延迟等待布局完成)
        QTimer::singleShot(0, this, [this, pos = cache.scrollPosition]() {
            contentGrid->verticalScrollBar()->setValue(pos);
            onContentViewportChanged(); // 触发一次检查，万一视口变了
        });

        // 更新追踪变量
        m_lastLoadedPath = currentPath;
        contentGrid->setUpdatesEnabled(true);
        return; // <--- 直接返回，不再执行耗时的文件扫描
    }

    // ---------------------------------------------------------
    // C. [全量加载] 如果没有缓存，走常规流程
    // ---------------------------------------------------------
    thumbReady.clear(); // 清空已就绪标记

    QDir dir(currentPath);
    QStringList filters;
    if (checkImages->isChecked())
        filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp";
    if (checkVideos->isChecked())
        filters << "*.mp4" << "*.mkv" << "*.avi" << "*.mov";

    dir.setNameFilters(filters);
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                           QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &info : list) {
        QListWidgetItem *item = new QListWidgetItem(info.fileName());
        const QString filePath = info.absoluteFilePath();
        const QString suffix   = info.suffix().toLower();
        const bool isVideo     = (suffix == "mp4" || suffix == "mkv"
                              || suffix == "avi" || suffix == "mov");

        item->setData(Qt::UserRole,     filePath);
        item->setData(Qt::UserRole + 1, isVideo);
        item->setData(Qt::UserRole + 10, videoTags.value(filePath));

        item->setIcon(isVideo
                          ? style()->standardIcon(QStyle::SP_MediaPlay)
                          : style()->standardIcon(QStyle::SP_FileIcon));

        contentGrid->addItem(item);
    }

    m_lastLoadedPath = currentPath; // 更新追踪变量
    contentGrid->setUpdatesEnabled(true);

    QTimer::singleShot(0, this, [this]() { onContentViewportChanged(); });
}

void YouTubeStyleManager::startThumbnailGeneration()
{
    if (pendingThumbTasks.isEmpty())
        return;

    // 再次防御：如果 watcher 正在监控旧任务，先取消
    if (iconWatcher->isRunning())
        iconWatcher->cancel();

    // 统一缩略图尺寸
    const int THUMB_WIDTH = 320;
    const int THUMB_HEIGHT = 240;

    auto future = QtConcurrent::mapped(pendingThumbTasks,
                                       [=](const LoadTask &task) {
                                           QIcon icon;

                                           if (task.isVideo) {
                                               QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
                                               QDir().mkpath(cacheDir);

                                               QByteArray hash = QCryptographicHash::hash(
                                                   task.path.toUtf8(), QCryptographicHash::Md5);
                                               QString cacheFile = cacheDir + "/thumb_" + hash.toHex() + ".jpg";

                                               if (!QFile::exists(cacheFile)) {
                                                   // ffmpeg 截图并缩放
                                                   QProcess::execute(ffmpegExecutablePath(),
                                                                     QStringList()
                                                                         << "-ss" << "5"
                                                                         << "-i" << task.path
                                                                         << "-frames:v" << "1"
                                                                         << "-q:v" << "5"
                                                                         << "-threads" << "1"
                                                                         << "-vf"
                                                                         << QString("scale=%1:-1").arg(THUMB_WIDTH)
                                                                         << cacheFile << "-y");
                                               }

                                               if (QFile::exists(cacheFile)) {
                                                   QImageReader reader(cacheFile);
                                                   if (reader.canRead()) {
                                                       QSize size = reader.size();
                                                       if (size.isValid()) {
                                                           reader.setScaledSize(
                                                               size.scaled(THUMB_WIDTH, THUMB_HEIGHT,
                                                                           Qt::KeepAspectRatio));
                                                       }
                                                       QImage img = reader.read();
                                                       if (!img.isNull())
                                                           icon = QIcon(QPixmap::fromImage(img));
                                                   }
                                               }
                                           } else {
                                               QImageReader reader(task.path);
                                               reader.setAutoTransform(true);

                                               if (reader.canRead()) {
                                                   QSize size = reader.size();
                                                   if (size.isValid()) {
                                                       reader.setScaledSize(
                                                           size.scaled(THUMB_WIDTH, THUMB_HEIGHT,
                                                                       Qt::KeepAspectRatio));
                                                   }
                                                   QImage img = reader.read();
                                                   if (!img.isNull())
                                                       icon = QIcon(QPixmap::fromImage(img));
                                               }
                                           }

                                           return qMakePair(task.index, icon);
                                       });

    iconWatcher->setFuture(future);
}

void YouTubeStyleManager::openFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", currentPath);
    if (!dir.isEmpty()) {
        currentPath = dir;
        pathLabel->setText(dir);
        loadContent();
        rebuildFolderList();    // 根路径变更后，子文件夹列表一起刷新
        updateBackButtonState();

        // 更新目录监视器监听的路径
        if (dirWatcher) {
            dirWatcher->removePaths(dirWatcher->directories());
            dirWatcher->addPath(currentPath);
        }
    }
}

void YouTubeStyleManager::handleItemClicked(QListWidgetItem *item) {
    QString filePath = item->data(Qt::UserRole).toString();
    QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == "mp4" || suffix == "mkv" || suffix == "avi") {
        detailPage->setVideoPath(filePath);

        QStringList tags = item->data(Qt::UserRole + 10).toStringList();
        detailPage->setTags(tags);

        mainStack->setCurrentWidget(detailPage);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void YouTubeStyleManager::showBrowser() {
    mainStack->setCurrentWidget(browserPage);
}

void YouTubeStyleManager::filterContent(const QString &text) {
    QString keyword = text.trimmed();

    for (int i = 0; i < contentGrid->count(); ++i) {
        QListWidgetItem *item = contentGrid->item(i);

        // 搜索框为空：全部显示
        if (keyword.isEmpty()) {
            item->setHidden(false);
            continue;
        }

        // 1) 文件名匹配
        bool matchName = item->text().contains(keyword, Qt::CaseInsensitive);

        // 2) 标签匹配（标签存放在 UserRole + 10）
        QStringList tags = item->data(Qt::UserRole + 10).toStringList();
        bool matchTag = false;
        for (const QString &tag : tags) {
            if (tag.contains(keyword, Qt::CaseInsensitive)) {
                matchTag = true;
                break;
            }
        }

        bool visible = matchName || matchTag;
        item->setHidden(!visible);
    }
}

// 计算标签 JSON 文件的路径
QString YouTubeStyleManager::tagFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    QDir().mkpath(dir);
    return dir + "/video_tags.json";
}

// 启动时从 JSON 文件加载所有标签
void YouTubeStyleManager::loadTags()
{
    videoTags.clear();

    QFile f(tagFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QByteArray data = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString path = it.key();
        const QJsonArray arr = it.value().toArray();
        QStringList tags;
        for (const QJsonValue &v : arr)
            tags << v.toString();
        videoTags.insert(path, tags);
    }
}

// 将当前 videoTags 写回 JSON 文件
void YouTubeStyleManager::saveTags() const
{
    QJsonObject obj;
    for (auto it = videoTags.constBegin(); it != videoTags.constEnd(); ++it) {
        QJsonArray arr;
        for (const QString &tag : it.value())
            arr.append(tag);
        obj.insert(it.key(), arr);
    }

    const QJsonDocument doc(obj);
    QFile f(tagFilePath());
    if (!f.open(QIODevice::WriteOnly))
        return;

    f.write(doc.toJson(QJsonDocument::Indented));
}

// 响应详情页的标签变更
void YouTubeStyleManager::updateVideoTags(const QString &path,
                                          const QStringList &tags)
{
    if (tags.isEmpty())
        videoTags.remove(path);
    else
        videoTags[path] = tags;

    // 更新列表项上的标签数据
    for (int i = 0; i < contentGrid->count(); ++i) {
        QListWidgetItem *item = contentGrid->item(i);
        if (item->data(Qt::UserRole).toString() == path) {
            item->setData(Qt::UserRole + 10, tags);
            break;
        }
    }

    saveTags();

    // 重新应用当前搜索过滤
    if (searchEdit)
        filterContent(searchEdit->text());
}

bool YouTubeStyleManager::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == contentGrid->viewport()) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            scrollDebounceTimer->start();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void YouTubeStyleManager::onContentViewportChanged()
{
    if (!contentGrid || contentGrid->count() == 0)
        return;

    QWidget *vp = contentGrid->viewport();
    if (!vp)
        return;

    QRect vpRect = vp->rect();
    if (vpRect.isEmpty())
        return;

    // 1. 扩大视口区域（上下各预加载一屏），保证滚动时不会露白
    const int extra = vpRect.height();
    vpRect.adjust(0, -extra, 0, extra);

    const int itemCount = contentGrid->count();

    // === 核心优化 START ===

    // 2. 智能定位起点：不再从 0 开始傻傻遍历
    // contentGrid->itemAt(x, y) 获取视口当前可见位置的 Item
    // 我们探测视口左上角的 item，从而知道当前滚动到了哪里
    int start = 0;
    // 尝试探测左上角
    QListWidgetItem *topItem = contentGrid->itemAt(20, 20);
    // 如果左上角没踩到（可能是padding），尝试探测视口中心
    if (!topItem) {
        topItem = contentGrid->itemAt(vpRect.center());
        // 如果中心点踩到了，我们往回倒推一些，确保覆盖前面
        if (topItem) {
            // 拿到中心元素的索引，稍微多减一点，比如减100，保证回溯到顶部
            start = qMax(0, contentGrid->row(topItem) - 100);
        }
    } else {
        start = contentGrid->row(topItem);
        start = qMax(0, start - 60);
    }

    // 从计算出的 start 开始遍历，而不是从 0 开始
    for (int i = start; i < itemCount; ++i) {
        // 已经生成过或正在生成的，跳过
        if (thumbReady.contains(i) || thumbRequested.contains(i))
            continue;

        QListWidgetItem *item = contentGrid->item(i);
        if (!item) continue;

        // 获取 item 在视口中的几何位置
        // 注意：visualItemRect 在大量调用时有性能开销，但因为我们限制了循环次数，所以这里很快
        const QRect itemRect = contentGrid->visualItemRect(item);

        // 3. 【关键刹车】如果当前 Item 的顶部已经跑到了视口下方
        // 说明后面的几千个 Item 肯定也在更下方，直接 break 终止循环！
        // 这样每次滚动只需要计算 100 次左右，而不是 10000 次。
        if (itemRect.top() > vpRect.bottom()) {
            break;
        }

        // 4. 如果 Item 还在视口上方（因为我们回退了 60 个），则跳过
        if (itemRect.bottom() < vpRect.top()) {
            continue;
        }

        // 5. 命中：在扩展视口内 -> 加入任务队列
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) continue;

        const bool isVideo = item->data(Qt::UserRole + 1).toBool();

        LoadTask task;
        task.index   = i;
        task.path    = path;
        task.isVideo = isVideo;

        thumbTaskQueue.enqueue(task);
        thumbRequested.insert(i);
    }
    // === 核心优化 END ===

    tryStartNextThumbBatch();
}

void YouTubeStyleManager::tryStartNextThumbBatch()
{
    // 已有一个 future 在跑，就不要再开新的
    if (iconWatcher->isRunning())
        return;

    if (thumbTaskQueue.isEmpty())
        return;

    QList<LoadTask> batch;
    QRect viewportRect = contentGrid->viewport()->rect(); // 获取当前视口
    while (!thumbTaskQueue.isEmpty() && batch.size() < ThumbBatchSize) {
        LoadTask task = thumbTaskQueue.dequeue();

        // --- 新增优化：二次可见性检查 ---
        QListWidgetItem* item = contentGrid->item(task.index);
        if (item) {
            QRect itemRect = contentGrid->visualItemRect(item);
            // 如果任务取出来时，已经不在视口内了（比如用户又滑走了），直接丢弃！
            // 注意：这里放宽一点判定范围，避免边缘闪烁
            if (itemRect.bottom() < -500 || itemRect.top() > viewportRect.height() + 500) {
                thumbRequested.remove(task.index); // 移除占用标记
                continue; // 跳过此任务，取下一个
            }
        }
        // -----------------------------

        batch.append(task);
    }

    if (batch.isEmpty())
        return;

    const int THUMB_WIDTH  = 320;
    const int THUMB_HEIGHT = 240;

    auto future = QtConcurrent::mapped(batch, [=](const LoadTask &task) {
        QIcon icon;

        if (task.isVideo) {
            QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            QDir().mkpath(cacheDir);

            QByteArray hash = QCryptographicHash::hash(
                task.path.toUtf8(), QCryptographicHash::Md5);
            QString cacheFile = cacheDir + "/thumb_" + hash.toHex() + ".jpg";

            if (!QFile::exists(cacheFile)) {
                QStringList args;
                args << "-ss" << "5"
                     << "-i" << task.path
                     << "-frames:v" << "1"
                     << "-q:v" << "5"
                     << "-threads" << "1"
                     << "-vf" << QString("scale=%1:-1").arg(THUMB_WIDTH)
                     << cacheFile << "-y";

                // 这里在后台线程调用，不会阻塞 UI
                runFfmpegBlocking(args);
            }

            if (QFile::exists(cacheFile)) {
                QImageReader reader(cacheFile);
                if (reader.canRead()) {
                    QSize size = reader.size();
                    if (size.isValid()) {
                        reader.setScaledSize(
                            size.scaled(THUMB_WIDTH, THUMB_HEIGHT,
                                        Qt::KeepAspectRatio));
                    }
                    QImage img = reader.read();
                    if (!img.isNull())
                        icon = QIcon(QPixmap::fromImage(img));
                }
            }
        } else {
            QImageReader reader(task.path);
            reader.setAutoTransform(true);

            if (reader.canRead()) {
                QSize size = reader.size();
                if (size.isValid()) {
                    reader.setScaledSize(
                        size.scaled(THUMB_WIDTH, THUMB_HEIGHT,
                                    Qt::KeepAspectRatio));
                }
                QImage img = reader.read();
                if (!img.isNull())
                    icon = QIcon(QPixmap::fromImage(img));
            }
        }

        return qMakePair(task.index, icon);
    });

    iconWatcher->setFuture(future);
}
