#include "YouTubeStyleManager.h"
#include "ThumbnailDelegate.h"
#include "VideoDetailWidget.h"

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

YouTubeStyleManager::YouTubeStyleManager(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("XSimple Media Manager");
    resize(1280, 800);

    // 初始路径：视频目录，不存在则使用 home
    currentPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (currentPath.isEmpty()) currentPath = QDir::homePath();

    // 异步缩略图 watcher
    iconWatcher = new QFutureWatcher<QPair<int, QIcon>>(this);
    connect(iconWatcher, &QFutureWatcher<QPair<int, QIcon>>::resultReadyAt,
            this, &YouTubeStyleManager::onThumbnailLoaded);

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

    QPushButton *btnOpen = new QPushButton("打开文件夹", this);
    btnOpen->setObjectName("sidebarBtn");
    btnOpen->setCursor(Qt::PointingHandCursor);
    leftLayout->addWidget(btnOpen);

    leftLayout->addStretch();
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
    rightLayout->addWidget(contentGrid);

    mainLayout->addWidget(rightPanel);

    // 详情页
    detailPage = new VideoDetailWidget(this);
    mainStack->addWidget(browserPage);
    mainStack->addWidget(detailPage);

    // 信号连接
    connect(checkImages, &QCheckBox::toggled, this, &YouTubeStyleManager::loadContent);
    connect(checkVideos, &QCheckBox::toggled, this, &YouTubeStyleManager::loadContent);
    connect(btnOpen, &QPushButton::clicked, this, &YouTubeStyleManager::openFolder);
    connect(contentGrid, &QListWidget::itemClicked,
            this, &YouTubeStyleManager::handleItemClicked);
    connect(detailPage, &VideoDetailWidget::backRequested,
            this, &YouTubeStyleManager::showBrowser);

    // 详情页标签变更信号
    connect(detailPage, &VideoDetailWidget::tagsChanged,
            this, &YouTubeStyleManager::updateVideoTags);

    applyStyle();

    // 启动时加载已经保存的标签
    loadTags();
    loadContent();
}

YouTubeStyleManager::~YouTubeStyleManager() {}

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
    this->setStyleSheet(qss);
}

void YouTubeStyleManager::onThumbnailLoaded(int index) {
    if (index >= 0 && index < contentGrid->count()) {
        QPair<int, QIcon> result = iconWatcher->resultAt(index);
        QListWidgetItem *item = contentGrid->item(result.first);
        // 只有当结果有效时才更新，否则保持默认图标
        if (item && !result.second.isNull())
            item->setIcon(result.second);
    }
}

void YouTubeStyleManager::loadContent() {
    contentGrid->clear();
    if (iconWatcher->isRunning())
        iconWatcher->cancel();

    QDir dir(currentPath);
    QStringList filters;
    if (checkImages->isChecked()) filters << "*.jpg" << "*.png" << "*.jpeg";
    if (checkVideos->isChecked()) filters << "*.mp4" << "*.mkv" << "*.avi";

    dir.setNameFilters(filters);
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    // 任务结构
    struct LoadTask {
        int index;
        QString path;
        bool isVideo;
    };
    QList<LoadTask> tasks;

    int row = 0;
    for (const QFileInfo &info : list) {
        QListWidgetItem *item = new QListWidgetItem(info.fileName());
        QString filePath = info.absoluteFilePath();
        QString suffix = info.suffix().toLower();
        bool isVideo = (suffix == "mp4" || suffix == "mkv" || suffix == "avi");

        item->setData(Qt::UserRole, info.absoluteFilePath());       // 路径
        item->setData(Qt::UserRole + 1, isVideo);    // 是否视频（供 delegate/别处使用）
        item->setData(Qt::UserRole + 10, videoTags.value(filePath)); // 标签

        // 标签用单独的角色，避免冲突
        item->setData(Qt::UserRole + 10, videoTags.value(info.absoluteFilePath()));

        // 默认图标
        item->setIcon(isVideo
                          ? style()->standardIcon(QStyle::SP_MediaPlay)
                          : style()->standardIcon(QStyle::SP_FileIcon));

        // 加入异步任务列表
        tasks.append({row, info.absoluteFilePath(), isVideo});

        contentGrid->addItem(item);
        row++;
    }

    if (!tasks.isEmpty()) {
        // 统一缩略图尺寸
        const int THUMB_WIDTH = 320;
        const int THUMB_HEIGHT = 240;

        auto future = QtConcurrent::mapped(tasks, [=](const LoadTask &task) {
            QIcon icon;

            if (task.isVideo) {
                QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
                QDir().mkpath(cacheDir);

                QByteArray hash = QCryptographicHash::hash(
                    task.path.toUtf8(), QCryptographicHash::Md5);
                QString cacheFile = cacheDir + "/thumb_" + hash.toHex() + ".jpg";

                if (!QFile::exists(cacheFile)) {
                    // ffmpeg 截图并缩放
                    QProcess::execute("ffmpeg",
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
}

void YouTubeStyleManager::openFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", currentPath);
    if (!dir.isEmpty()) {
        currentPath = dir;
        pathLabel->setText(dir);
        loadContent();
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
