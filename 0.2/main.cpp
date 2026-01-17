#include "YouTubeStyleManager.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QStandardPaths>
#include <QPainter>
#include <QFileDialog>
#include <QFileInfo>
#include <QStyle>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QFontMetrics>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QImageReader>
#include <QCryptographicHash>
#include <QRandomGenerator>

// =========================================================
// Helper: ThumbnailDelegate
// 核心升级：支持图片比例自适应，完美居中绘制
// =========================================================
// 优化后的 Delegate
class ThumbnailDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 获取数据
        QString text = index.data(Qt::DisplayRole).toString();
        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

        QRect rect = option.rect;
        rect.adjust(6, 6, -6, -6); // 边距

        // 1. 绘制背景 (选中/悬停状态)
        QColor bgColor = Qt::transparent;
        if (option.state & QStyle::State_Selected) {
            bgColor = QColor("#3ea6ff"); // 选中蓝
        } else if (option.state & QStyle::State_MouseOver) {
            bgColor = QColor("#2d2d2d"); // 悬停深灰
        }

        if (bgColor != Qt::transparent) {
            painter->setBrush(bgColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(rect, 8, 8);
        }

        // 2. 绘制缩略图
        // 预留底部 30px 给文字
        QRect contentRect(rect.left() + 8, rect.top() + 8, rect.width() - 16, rect.height() - 40);

        if (!icon.isNull()) {
            // 【优化点】：直接获取指定大小的 pixmap，避免在 paint 中进行 SmoothTransformation
            // 假设我们在加载时已经确保存储的是合适尺寸的图标，或者利用 QIcon 的缓存机制
            QPixmap pix = icon.pixmap(contentRect.size());

            // 计算居中
            int x = contentRect.left() + (contentRect.width() - pix.width()) / 2;
            int y = contentRect.top() + (contentRect.height() - pix.height()) / 2;

            painter->drawPixmap(x, y, pix);

            // 绘制视频播放三角形标记 (如果是视频)
            // 这里可以通过 UserRole 判断是否是视频，避免不必要的绘制
            bool isVideo = index.data(Qt::UserRole + 1).toBool();
            if (isVideo) {
                painter->setBrush(QColor(0, 0, 0, 150));
                painter->setPen(Qt::NoPen);
                painter->drawEllipse(contentRect.center(), 20, 20);

                painter->setBrush(Qt::white);
                QPoint center = contentRect.center();
                QPolygon tri;
                tri << QPoint(center.x() - 6, center.y() - 8)
                    << QPoint(center.x() - 6, center.y() + 8)
                    << QPoint(center.x() + 8, center.y());
                painter->drawPolygon(tri);
            }
        }

        // 3. 绘制文字 (使用 Elided 文本避免溢出)
        QRect textRect(rect.left() + 5, rect.bottom() - 30, rect.width() - 10, 25);
        painter->setPen((option.state & QStyle::State_Selected) ? Qt::black : QColor("#e0e0e0"));
        painter->setFont(QFont("Segoe UI", 9));

        QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignCenter, elidedText);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(180, 160);
    }
};

// =========================================================
// Helper: VideoDetailWidget
// 黑色背景，左侧Cover，右侧Info，下方截图
// =========================================================
class VideoDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoDetailWidget(QWidget *parent = nullptr) : QWidget(parent) {
        this->setStyleSheet("background-color: #000000; color: white;");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 顶部导航
        QWidget *topBar = new QWidget(this);
        topBar->setFixedHeight(50);
        topBar->setStyleSheet("background-color: #0f0f0f; border-bottom: 1px solid #333;");
        QHBoxLayout *topLayout = new QHBoxLayout(topBar);
        QPushButton *btnBack = new QPushButton("← 返回库", this);
        btnBack->setCursor(Qt::PointingHandCursor);
        btnBack->setStyleSheet("QPushButton { color: white; font-weight: bold; border: none; font-size: 15px; background: transparent; } QPushButton:hover { color: #3ea6ff; }");
        connect(btnBack, &QPushButton::clicked, this, &VideoDetailWidget::backRequested);
        topLayout->addWidget(btnBack);
        topLayout->addStretch();
        mainLayout->addWidget(topBar);

        // 内容区域
        QWidget *contentWidget = new QWidget(this);
        QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(40, 30, 40, 30);
        contentLayout->setSpacing(30);

        // 头部信息
        QHBoxLayout *headerInfoLayout = new QHBoxLayout();

        coverLabel = new QLabel(this);
        coverLabel->setFixedSize(400, 225);
        coverLabel->setStyleSheet("background-color: #111; border-radius: 8px; border: 1px solid #333;");
        coverLabel->setAlignment(Qt::AlignCenter);
        coverLabel->setText("No Cover");
        headerInfoLayout->addWidget(coverLabel);

        QVBoxLayout *infoRightLayout = new QVBoxLayout();
        infoRightLayout->setContentsMargins(30, 10, 0, 10);

        titleLabel = new QLabel("Title", this);
        titleLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: white;");
        titleLabel->setWordWrap(true);
        infoRightLayout->addWidget(titleLabel);

        infoLabel = new QLabel("--", this);
        infoLabel->setStyleSheet("font-size: 14px; color: #aaa; margin-top: 5px;");
        infoRightLayout->addWidget(infoLabel);

        infoRightLayout->addStretch();

        QPushButton *playBtn = new QPushButton("▶ 立即播放", this);
        playBtn->setFixedSize(160, 45);
        playBtn->setCursor(Qt::PointingHandCursor);
        playBtn->setStyleSheet("QPushButton { background-color: #3ea6ff; color: #000; font-weight: bold; border-radius: 4px; font-size: 15px; } QPushButton:hover { background-color: #65b8ff; }");
        connect(playBtn, &QPushButton::clicked, this, &VideoDetailWidget::playCurrentVideo);
        infoRightLayout->addWidget(playBtn);

        headerInfoLayout->addLayout(infoRightLayout);
        contentLayout->addLayout(headerInfoLayout);

        // 截图区域
        QLabel *secTitle = new QLabel("精彩预览", this);
        secTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd; margin-top: 20px;");
        contentLayout->addWidget(secTitle);

        QHBoxLayout *shotsArrayLayout = new QHBoxLayout();
        shotsArrayLayout->setSpacing(15);

        for (int i = 0; i < 5; i++) {
            QLabel *shot = new QLabel(this);
            shot->setFixedSize(192, 108);
            shot->setStyleSheet("background-color: #111; border-radius: 4px; border: 1px solid #333;");
            shot->setAlignment(Qt::AlignCenter);
            shot->setText(QString("#%1").arg(i+1));
            screenshotLabels.append(shot);
            shotsArrayLayout->addWidget(shot);
        }
        shotsArrayLayout->addStretch();
        contentLayout->addLayout(shotsArrayLayout);

        contentLayout->addStretch();
        mainLayout->addWidget(contentWidget);

        connect(&shotWatcher, &QFutureWatcher<void>::finished, this, [](){});
    }

    void setVideoPath(const QString &path) {
        currentVideoPath = path;
        QFileInfo info(path);
        titleLabel->setText(info.fileName());
        infoLabel->setText(QString("格式: %1 | 大小: %2 MB").arg(info.suffix().toUpper()).arg(info.size() / 1024.0 / 1024.0, 0, 'f', 1));

        coverLabel->setText("Loading...");
        for(auto l : screenshotLabels) { l->clear(); l->setText("..."); }
        generateScreenshots();
    }

private:
    void generateScreenshots() {
        if (shotWatcher.isRunning()) return;

        auto future = QtConcurrent::run([this, path = currentVideoPath]() {
            // === 1. 获取视频时长 (使用 ffprobe) ===
            QProcess probe;
            // ffprobe 命令：只输出时长(秒)，格式简洁
            probe.start("ffprobe", QStringList()
                                       << "-v" << "error"
                                       << "-show_entries" << "format=duration"
                                       << "-of" << "default=noprint_wrappers=1:nokey=1"
                                       << path);
            probe.waitForFinished();

            QString durationStr = probe.readAllStandardOutput().trimmed();
            double totalSeconds = durationStr.toDouble();

            // 容错处理：如果获取失败或时长极短，使用默认 3秒
            if (totalSeconds <= 10) totalSeconds = 3;

            // === 2. 随机生成 5 个时间点 ===
            QVector<int> timePoints;
            int shotCount = 5;
            double segment = totalSeconds / shotCount; // 将视频分为 5 段

            for (int i = 0; i < shotCount; i++) {
                // 在每一段内随机取一个点 (例如 0-20%, 20-40%...)
                // 使用 QRandomGenerator 生成随机偏移
                int minT = i * segment;
                int maxT = (i + 1) * segment;

                // 确保范围有效
                int range = maxT - minT;
                if (range <= 0) range = 1;

                int t = minT + QRandomGenerator::global()->bounded(range);

                // 修正：避免第0秒可能是黑屏，稍微往后挪一点
                if (t < 2) t = 2;
                if (t > totalSeconds - 1) t = totalSeconds - 1;

                timePoints.append(t);
            }

            QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

            // === 3. 生成封面 (取视频 10% 处，通常比开头更有代表性) ===
            int coverTime = totalSeconds * 0.10;
            if (coverTime < 5) coverTime = 5; // 最小 5秒

            QString coverShot = tempPath + "/cover_" + QFileInfo(path).fileName() + ".jpg";
            executeFFmpeg(path, coverTime, coverShot); // 使用动态计算的时间

            QMetaObject::invokeMethod(this, [this, coverShot](){
                QPixmap p(coverShot);
                if (!p.isNull()) coverLabel->setPixmap(p.scaled(coverLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            });

            // === 4. 生成详情预览图 ===
            for (int i = 0; i < timePoints.size(); i++) {
                int t = timePoints[i];
                // 文件名加上时间戳防止冲突
                QString shotPath = tempPath + QString("/shot_%1_%2_%3.jpg").arg(QFileInfo(path).fileName()).arg(i).arg(t);

                executeFFmpeg(path, t, shotPath);

                // 更新 UI
                QMetaObject::invokeMethod(this, [this, i, shotPath](){
                    if (i < screenshotLabels.size()) {
                        QPixmap p(shotPath);
                        if (!p.isNull()) screenshotLabels[i]->setPixmap(p.scaled(screenshotLabels[i]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                });
            }
        });
        shotWatcher.setFuture(future);
    }

    void executeFFmpeg(const QString &input, int seconds, const QString &output) {
        QProcess::execute("ffmpeg", QStringList() << "-ss" << QString::number(seconds) << "-i" << input << "-frames:v" << "1" << "-q:v" << "3" << output << "-y");
    }

private slots:
    void playCurrentVideo() {
        if (!currentVideoPath.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(currentVideoPath));
    }

signals:
    void backRequested();

private:
    QLabel *coverLabel;
    QLabel *titleLabel;
    QLabel *infoLabel;
    QList<QLabel*> screenshotLabels;
    QString currentVideoPath;
    QFutureWatcher<void> shotWatcher;
};

// =========================================================
// Main Class
// =========================================================

YouTubeStyleManager::YouTubeStyleManager(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("XSimple Media Manager");
    resize(1280, 800);

    currentPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (currentPath.isEmpty()) currentPath = QDir::homePath();

    iconWatcher = new QFutureWatcher<QPair<int, QIcon>>(this);
    connect(iconWatcher, &QFutureWatcher<QPair<int, QIcon>>::resultReadyAt, this, &YouTubeStyleManager::onThumbnailLoaded);

    mainStack = new QStackedWidget(this);
    setCentralWidget(mainStack);

    // Browser Page
    browserPage = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(browserPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left Panel
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(240);

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 10, 5, 10);
    leftLayout->setSpacing(5);

    QLabel *logo = new QLabel("MediaHub", this);
    logo->setStyleSheet("color: white; font-size: 20px; font-weight: bold; margin-bottom: 20px; padding-left: 10px;");
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

    // Right Panel
    QWidget *rightPanel = new QWidget(this);
    rightPanel->setStyleSheet("background-color: #0f0f0f;");
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

    // === 创建顶部工具栏布局 ===
    QHBoxLayout *topBarLayout = new QHBoxLayout();

    pathLabel = new QLabel(currentPath, this);
    pathLabel->setStyleSheet("color: #aaaaaa; font-size: 14px; font-family: 'Segoe UI'; font-weight: 500;");
    rightLayout->addWidget(pathLabel);

    // 添加弹簧 (将搜索框推到最右侧，或者如果想紧挨着路径，可以去掉这个 stretch)
    topBarLayout->addStretch();
    // 3. 搜索框 (放在右侧，与路径平齐)
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("搜索...");
    searchEdit->setFixedWidth(240); // 保持固定长度
    searchEdit->setClearButtonEnabled(true); // 显示清除按钮
    // 设置搜索框样式 (YouTube 风格深色主题)
    // 设置圆角样式
    searchEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: #121212;"      /* 深色背景 */
        "    border: 1px solid #333333;"      /* 边框颜色 */
        "    border-radius: 18px;"            /* 圆角半径：高度的一半左右实现胶囊形 */
        "    color: #ffffff;"                 /* 文字颜色 */
        "    padding: 8px 15px;"              /* 内边距 */
        "    font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #3ea6ff;"      /* 聚焦时边框变蓝 */
        "    background-color: #1a1a1a;"
        "}"
        );
    // 连接信号：当文本改变时触发筛选
    connect(searchEdit, &QLineEdit::textChanged, this, &YouTubeStyleManager::filterContent);

    topBarLayout->addWidget(searchEdit);
    // 设置顶部边距为 8px (些许缝隙)，左右 20px，底部 5px
    topBarLayout->setContentsMargins(20, 8, 20, 5);

    // 将顶部工具栏添加到右侧主布局
    rightLayout->addLayout(topBarLayout);

    contentGrid = new QListWidget(this);
    contentGrid->setViewMode(QListWidget::IconMode);
    contentGrid->setResizeMode(QListWidget::Adjust);
    contentGrid->setSpacing(12);
    contentGrid->setIconSize(QSize(180, 120)); // Icon Size
    contentGrid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentGrid->setStyleSheet("QListWidget { background-color: #0f0f0f; border: none; outline: none; } QListWidget::item { color: #f1f1f1; }");
    contentGrid->setItemDelegate(new ThumbnailDelegate(this));

    rightLayout->addWidget(contentGrid);
    mainLayout->addWidget(rightPanel);

    detailPage = new VideoDetailWidget(this);
    mainStack->addWidget(browserPage);
    mainStack->addWidget(detailPage);

    connect(checkImages, &QCheckBox::toggled, this, &YouTubeStyleManager::loadContent);
    connect(checkVideos, &QCheckBox::toggled, this, &YouTubeStyleManager::loadContent);
    connect(btnOpen, &QPushButton::clicked, this, &YouTubeStyleManager::openFolder);
    connect(contentGrid, &QListWidget::itemClicked, this, &YouTubeStyleManager::handleItemClicked);
    connect(detailPage, &VideoDetailWidget::backRequested, this, &YouTubeStyleManager::showBrowser);

    applyStyle();
    loadContent();
}

YouTubeStyleManager::~YouTubeStyleManager() {}

// 恢复复选框 UI 设计
void YouTubeStyleManager::applyStyle() {
    QString qss = R"(
        QMainWindow, QWidget { background-color: #0f0f0f; color: #f1f1f1; font-family: "Segoe UI", sans-serif; font-size: 14px; }
        QWidget#leftPanel { background-color: #050505; }
        QCheckBox#sidebarItem {
            background-color: transparent; color: #aaaaaa; padding: 12px 20px; border-radius: 10px; font-size: 15px; margin-bottom: 2px; spacing: 15px;
        }
        QCheckBox::indicator#sidebarItem { width: 0px; height: 0px; border: none; }
        QCheckBox#sidebarItem:hover { background-color: #1f1f1f; color: white; }
        QCheckBox#sidebarItem:checked { background-color: #1f1f1f; color: white; font-weight: bold; border-left: 3px solid #3ea6ff; }
        QPushButton#sidebarBtn { background-color: transparent; color: #aaaaaa; padding: 12px 20px; border-radius: 10px; text-align: left; border: none; font-size: 15px; }
        QPushButton#sidebarBtn:hover { background-color: #1f1f1f; color: white; }
    )";
    this->setStyleSheet(qss);
}

void YouTubeStyleManager::onThumbnailLoaded(int index) {
    if (index >= 0 && index < contentGrid->count()) {
        QPair<int, QIcon> result = iconWatcher->resultAt(index);
        QListWidgetItem *item = contentGrid->item(result.first);
        // 只有当结果有效时才更新，否则保持默认图标
        if (item && !result.second.isNull()) item->setIcon(result.second);
    }
}

void YouTubeStyleManager::loadContent() {
    contentGrid->clear();
    if (iconWatcher->isRunning()) iconWatcher->cancel();

    QDir dir(currentPath);
    QStringList filters;
    if (checkImages->isChecked()) filters << "*.jpg" << "*.png" << "*.jpeg";
    if (checkVideos->isChecked()) filters << "*.mp4" << "*.mkv" << "*.avi";

    dir.setNameFilters(filters);
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    // 定义任务结构：Index, FilePath, IsVideo
    struct LoadTask { int index; QString path; bool isVideo; };
    QList<LoadTask> tasks;

    int row = 0;
    for (const QFileInfo &info : list) {
        QListWidgetItem *item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());

        QString suffix = info.suffix().toLower();
        bool isVideo = (suffix == "mp4" || suffix == "mkv" || suffix == "avi");

        // 默认图标
        item->setIcon(isVideo ? style()->standardIcon(QStyle::SP_MediaPlay) : style()->standardIcon(QStyle::SP_FileIcon));

        // 加入异步任务列表
        tasks.append({row, info.absoluteFilePath(), isVideo});

        contentGrid->addItem(item);
        row++;
    }

    if (!tasks.isEmpty()) {
        // === 优化开始：定义统一的缩略图尺寸 ===
        const int THUMB_WIDTH = 320;
        const int THUMB_HEIGHT = 240;

        // 异步映射函数：处理图片读取和视频截图
        auto future = QtConcurrent::mapped(tasks, [=](const LoadTask &task) {
            QIcon icon;

            // --- 场景 A: 视频处理 ---
            if (task.isVideo) {
                QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
                QDir().mkpath(cacheDir);

                QByteArray hash = QCryptographicHash::hash(task.path.toUtf8(), QCryptographicHash::Md5);
                QString cacheFile = cacheDir + "/thumb_" + hash.toHex() + ".jpg";

                // 如果缓存不存在，调用 ffmpeg 生成
                if (!QFile::exists(cacheFile)) {
                    // 【优化点 1】：添加 -threads 1 限制线程，防止 UI 卡死
                    // 【优化点 2】：添加 -q:v 5 稍微降低质量以提升生成速度
                    // 【优化点 3】：scale=320:-1 确保生成的文件很小
                    QProcess::execute("ffmpeg", QStringList()
                                                    << "-ss" << "5"
                                                    << "-i" << task.path
                                                    << "-frames:v" << "1"
                                                    << "-q:v" << "5"
                                                    << "-threads" << "1"
                                                    << "-vf" << QString("scale=%1:-1").arg(THUMB_WIDTH)
                                                    << cacheFile << "-y");
                }

                // 加载缓存
                if (QFile::exists(cacheFile)) {
                    QImageReader reader(cacheFile);
                    if (reader.canRead()) {
                        // 【优化点 4】：即使是缓存文件，读取时也限制内存大小
                        QSize size = reader.size();
                        if (size.isValid()) {
                            reader.setScaledSize(size.scaled(THUMB_WIDTH, THUMB_HEIGHT, Qt::KeepAspectRatio));
                        }
                        QImage img = reader.read();
                        if (!img.isNull()) icon = QIcon(QPixmap::fromImage(img));
                    }
                }
            }
            // --- 场景 B: 图片处理 ---
            else {
                QImageReader reader(task.path);
                // 【优化点 5】：处理手机照片的旋转信息 (EXIF)
                reader.setAutoTransform(true);

                if (reader.canRead()) {
                    QSize size = reader.size();
                    if (size.isValid()) {
                        // 【优化点 6】：关键内存优化！
                        // 在解码前设置目标尺寸，避免将 4K/8K 原图读入内存
                        reader.setScaledSize(size.scaled(THUMB_WIDTH, THUMB_HEIGHT, Qt::KeepAspectRatio));

                        QImage img = reader.read();
                        if (!img.isNull()) {
                            icon = QIcon(QPixmap::fromImage(img));
                        }
                    }
                }
            }
            return qMakePair(task.index, icon);
        });

        // 监听异步完成结果 (这部分保持原样，或者确保在此处连接 watcher)
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
        mainStack->setCurrentWidget(detailPage);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void YouTubeStyleManager::showBrowser() {
    mainStack->setCurrentWidget(browserPage);
}

void YouTubeStyleManager::filterContent(const QString &text) {
    // 遍历所有列表项
    for (int i = 0; i < contentGrid->count(); ++i) {
        QListWidgetItem *item = contentGrid->item(i);
        // 检查文件名是否包含搜索文本 (Qt::CaseInsensitive 表示不区分大小写)
        bool matches = item->text().contains(text, Qt::CaseInsensitive);
        // 如果匹配则显示，不匹配则隐藏
        item->setHidden(!matches);
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    YouTubeStyleManager w;
    w.show();
    return app.exec();
}

#include "main.moc"
