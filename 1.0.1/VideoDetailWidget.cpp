#include "VideoDetailWidget.h"
#include "TagButton.h"
#include "FfmpegUtil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QProcess>
#include <QtConcurrent>
#include <QDir>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QMetaObject>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QCoreApplication>
#include <QThreadPool>
#include <QCryptographicHash>
#include <QMessageBox>      // 用于报错提示
#include <QDialog>          // 用于自定义大窗口
#include <QDialogButtonBox> // 用于确认/取消按钮
#include <QEvent>

VideoDetailWidget::VideoDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #000000; color: white;");

    // 初始化截图路径列表大小
    m_screenshotPaths.resize(5);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部导航
    QWidget *topBar = new QWidget(this);
    topBar->setFixedHeight(50);
    topBar->setStyleSheet(
        "background-color: #0f0f0f; border-bottom: 1px solid #333;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    QPushButton *btnBack = new QPushButton("← 返回库", this);
    btnBack->setCursor(Qt::PointingHandCursor);
    btnBack->setStyleSheet(
        "QPushButton { color: white; font-weight: bold; border: none;"
        " font-size: 15px; background: transparent; }"
        "QPushButton:hover { color: #3ea6ff; }");
    connect(btnBack, &QPushButton::clicked,
            this, &VideoDetailWidget::backRequested);
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
    coverLabel->setStyleSheet(
        "background-color: #111; border-radius: 8px; border: 1px solid #333;");
    coverLabel->setAlignment(Qt::AlignCenter | Qt::AlignTop);
    coverLabel->setText("No Cover");
    headerInfoLayout->addWidget(coverLabel);

    QVBoxLayout *infoRightLayout = new QVBoxLayout();
    infoRightLayout->setContentsMargins(30, 10, 0, 10);

    // === 标题区域增加重命名按钮 ===
    QHBoxLayout *titleRowLayout = new QHBoxLayout();
    titleRowLayout->setSpacing(10);

    titleLabel = new QLabel("Title", this);
    titleLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: white;");
    titleLabel->setWordWrap(true);
    titleRowLayout->addWidget(titleLabel, 1); // 标题占据主要空间

    QPushButton *renameBtn = new QPushButton("重命名", this);
    renameBtn->setCursor(Qt::PointingHandCursor);
    renameBtn->setStyleSheet(
        "QPushButton { background-color: #222; color: #aaa; border: 1px solid #444; border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
        "QPushButton:hover { background-color: #333; color: white; border-color: #666; }"
        );
    connect(renameBtn, &QPushButton::clicked, this, &VideoDetailWidget::onRenameClicked);
    titleRowLayout->addWidget(renameBtn, 0); // 按钮只占必要空间

    infoRightLayout->addLayout(titleRowLayout);

    infoLabel = new QLabel("--", this);
    infoLabel->setStyleSheet("font-size: 14px; color: #aaa; margin-top: 5px;");
    infoRightLayout->addWidget(infoLabel);

    // 标签区域： 左侧“标签：”文字 + 右侧方块标签 + “+”
    QHBoxLayout *tagsRowLayout = new QHBoxLayout();
    tagsRowLayout->setSpacing(8);

    QLabel *tagsLabel = new QLabel("标签：", this);
    tagsLabel->setStyleSheet("font-size: 14px; color: #ccc;");
    tagsRowLayout->addWidget(tagsLabel);

    tagLayout = new QHBoxLayout();
    tagLayout->setContentsMargins(0, 0, 0, 0);
    tagLayout->setSpacing(8);

    // 初始时没有标签，具体标签由 setTags() 填充
    tagsRowLayout->addLayout(tagLayout);
    tagsRowLayout->addStretch();

    infoRightLayout->addLayout(tagsRowLayout);

    infoRightLayout->addStretch();

    QPushButton *playBtn = new QPushButton("▶ 立即播放", this);
    playBtn->setFixedSize(160, 45);
    playBtn->setCursor(Qt::PointingHandCursor);
    playBtn->setStyleSheet(
        "QPushButton { background-color: #3ea6ff; color: #000;"
        " font-weight: bold; border-radius: 4px; font-size: 15px; }"
        "QPushButton:hover { background-color: #65b8ff; }");
    connect(playBtn, &QPushButton::clicked, this, &VideoDetailWidget::playCurrentVideo);
    infoRightLayout->addWidget(playBtn);

    headerInfoLayout->addLayout(infoRightLayout);
    contentLayout->addLayout(headerInfoLayout);

    // 截图区域标题
    QLabel *secTitle = new QLabel("精彩预览(点击查看大图)", this);
    secTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd; margin-top: 20px;");
    contentLayout->addWidget(secTitle);

    // 截图数组
    QHBoxLayout *shotsArrayLayout = new QHBoxLayout();
    shotsArrayLayout->setSpacing(15);

    for (int i = 0; i < 5; i++) {
        QLabel *shot = new QLabel(this);
        shot->setFixedSize(192, 108);
        shot->setStyleSheet("background-color: #111; border-radius: 4px; border: 1px solid #333;");
        shot->setAlignment(Qt::AlignCenter);
        shot->setText(QString("#%1").arg(i + 1));

        // 开启鼠标追踪并安装事件过滤器
        shot->setCursor(Qt::PointingHandCursor);
        shot->installEventFilter(this);

        screenshotLabels.append(shot);
        shotsArrayLayout->addWidget(shot);
    }
    shotsArrayLayout->addStretch();
    contentLayout->addLayout(shotsArrayLayout);

    contentLayout->addStretch();
    mainLayout->addWidget(contentWidget);

    // === 详情页专用线程池，只跑一个截图任务，防止与目录缩略图抢资源 ===
    detailThreadPool = new QThreadPool(this);
    detailThreadPool->setMaxThreadCount(1);
}

// 事件过滤器：处理图片点击
bool VideoDetailWidget::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        for (int i = 0; i < screenshotLabels.size(); ++i) {
            if (obj == screenshotLabels[i]) {
                // 如果对应位置有有效的截图文件路径
                if (i < m_screenshotPaths.size() && QFile::exists(m_screenshotPaths[i])) {
                    // 调用系统默认查看器打开图片
                    QDesktopServices::openUrl(QUrl::fromLocalFile(m_screenshotPaths[i]));
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// 重命名逻辑
void VideoDetailWidget::onRenameClicked() {
    if (currentVideoPath.isEmpty()) return;

    QFileInfo fileInfo(currentVideoPath);
    // 获取不带后缀的文件名（例如 "video.mp4" -> "video"）
    QString oldBaseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();

    // 1. 创建自定义对话框
    QDialog dlg(this);
    dlg.setWindowTitle("重命名视频");
    dlg.setMinimumWidth(600); // [关键] 设置窗口宽度为 600，满足“变大”的需求

    // 设置样式，保持与主程序一致的深色主题
    dlg.setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: white; border: 1px solid #333; }"
        "QLabel { font-size: 14px; color: #ccc; }"
        "QLineEdit { background-color: #2d2d2d; border: 1px solid #444; color: white; padding: 6px; border-radius: 4px; font-size: 15px; }"
        "QLineEdit:focus { border-color: #3ea6ff; }"
        "QPushButton { background-color: #333; color: white; border: 1px solid #555; padding: 6px 15px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #444; border-color: #666; }"
        );

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    // 提示标签
    QLabel *lbl = new QLabel(QString("请输入新的名称 (后缀 .%1 将自动保留):").arg(suffix), &dlg);
    layout->addWidget(lbl);

    // 输入框：只显示文件名，不显示后缀
    QLineEdit *edit = new QLineEdit(oldBaseName, &dlg);
    edit->selectAll(); // 默认全选，方便直接修改
    layout->addWidget(edit);

    // 确认/取消按钮
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btns);

    // 2. 显示并等待结果
    if (dlg.exec() == QDialog::Accepted) {
        QString newBaseName = edit->text().trimmed();

        // 如果名字没变或为空，直接返回
        if (newBaseName.isEmpty() || newBaseName == oldBaseName) return;

        // 非法字符检查
        if (newBaseName.contains("/") || newBaseName.contains("\\") || newBaseName.contains(":") ||
            newBaseName.contains("*") || newBaseName.contains("?") || newBaseName.contains("\"") ||
            newBaseName.contains("<") || newBaseName.contains(">") || newBaseName.contains("|")) {
            QMessageBox::warning(this, "错误", "文件名包含非法字符！");
            return;
        }

        // 3. 拼接后缀
        QString newFileName = newBaseName + "." + suffix;
        QString newPath = fileInfo.dir().filePath(newFileName);

        // 4. 执行重命名
        QFile file(currentVideoPath);
        if (file.rename(newPath)) {
            QString oldPath = currentVideoPath;

            // 更新界面
            setVideoPath(newPath);

            // 发送信号通知主界面更新列表
            emit videoRenamed(oldPath, newPath);
        } else {
            QMessageBox::critical(this, "失败", "无法重命名文件，可能文件正在被占用或名称冲突。");
        }
    }
}

void VideoDetailWidget::setVideoPath(const QString &path) {
    currentVideoPath = path;
    QFileInfo info(path);
    titleLabel->setText(info.fileName());
    infoLabel->setText(QString("格式: %1 | 大小: %2 MB")
                           .arg(info.suffix().toUpper())
                           .arg(info.size() / 1024.0 / 1024.0, 0, 'f', 1));

    // 先尝试用目录缩略图缓存作为初始封面，避免首次全黑
    bool coverSet = false;
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cacheDir.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5);
        QString cacheFile = cacheDir + "/thumb_" + hash.toHex() + ".jpg";
        if (QFile::exists(cacheFile)) {
            QPixmap p(cacheFile);
            if (!p.isNull()) {
                coverLabel->setPixmap(
                    p.scaled(coverLabel->size(),
                             Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation));
                coverSet = true;
            }
        }
    }
    if (!coverSet) {
        coverLabel->setText("Loading...");
    }

    for (auto l : screenshotLabels) {
        l->clear();
        l->setText("...");
    }

    generateScreenshots();
}

void VideoDetailWidget::setTags(const QStringList &tags) {
    m_tags = tags;

    // 安全重置指针，因为布局清理会删除实际的 widget
    m_tagInput = nullptr;

    if (!tagLayout) return;

    // 清空旧布局
    QLayoutItem *item;
    while ((item = tagLayout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater(); // 安全删除
        }
        delete item;
    }

    // 重新添加标签按钮
    for (const QString &t : m_tags) {
        TagButton *tagBtn = new TagButton(t, this);
        connect(tagBtn, &TagButton::deleted, this, [this](const QString &text) {
            m_tags.removeAll(text);
            setTags(m_tags);
            emit tagsChanged(currentVideoPath, m_tags);
        });
        tagLayout->addWidget(tagBtn);
    }

    // 添加 "+" 按钮
    QPushButton *addBtn = new QPushButton("+", this);
    addBtn->setFixedSize(20, 20);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #1e1e1e; border: 1px dashed #555;"
        "   border-radius: 4px; color: #aaa; font-size: 16px; padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        "   border-color: #3ea6ff; color: white;"
        "}");

    connect(addBtn, &QPushButton::clicked, this, [this, addBtn]() {
        showTagInput(addBtn);
    });

    tagLayout->addWidget(addBtn);
    tagLayout->addStretch();
}

void VideoDetailWidget::showTagInput(QPushButton *addBtn) {
    if (m_tagInput) return; // 防止重复点击

    addBtn->hide(); // 隐藏 "+" 按钮

    // 创建行内输入框
    m_tagInput = new QLineEdit(this);
    m_tagInput->setPlaceholderText("Tag");
    m_tagInput->setFixedWidth(80);
    m_tagInput->setMaxLength(15);
    // 蓝色边框表示焦点
    m_tagInput->setStyleSheet(
        "QLineEdit {"
        "   background-color: #1e1e1e; border: 1px solid #3ea6ff;"
        "   border-radius: 4px; color: white; font-size: 13px;"
        "   padding: 0 4px;"
        "}"
        );

    // 在 "+" 按钮原位置插入
    int index = tagLayout->indexOf(addBtn);
    if (index >= 0) {
        tagLayout->insertWidget(index, m_tagInput);
        m_tagInput->show();
        m_tagInput->setFocus();
    }

    // 连接信号
    // 使用 Old Syntax 或者 disconnect 策略来防止双重触发
    connect(m_tagInput, &QLineEdit::returnPressed,
            this, &VideoDetailWidget::handleTagInputFinished);

    // 失去焦点也视为确认（如果想按 ESC 取消，通常结合 eventFilter，但这里简单处理）
    connect(m_tagInput, &QLineEdit::editingFinished,
            this, &VideoDetailWidget::handleTagInputFinished);
}

void VideoDetailWidget::handleTagInputFinished() {
    // 检查指针有效性
    if (!m_tagInput) return;

    // 【关键改进】立即断开所有信号连接
    // 防止 "returnPressed" 和 "editingFinished" 连续触发导致两次调用
    // 或是 setTags 删除对象后再次触发信号导致 Crash
    m_tagInput->disconnect();

    QString newTag = m_tagInput->text().trimmed();
    bool added = false;

    // 简单的校验逻辑
    if (!newTag.isEmpty() && !m_tags.contains(newTag)) {
        m_tags.append(newTag);
        added = true;
    }

    // 重新构建 UI (在此过程中 m_tagInput 会被 deleteLater)
    setTags(m_tags);

    // 发送信号
    if (added) {
        emit tagsChanged(currentVideoPath, m_tags);
    }
}

void VideoDetailWidget::generateScreenshots() {
    // 不再阻止新的任务，让新视频可以覆盖旧视频的截图
    auto future = QtConcurrent::run(detailThreadPool, [this, path = currentVideoPath]() {
        // 1. ffprobe 获取时长
        QProcess probe;
        probe.start("ffprobe",
                    QStringList()
                        << "-v" << "error"
                        << "-show_entries" << "format=duration"
                        << "-of" << "default=noprint_wrappers=1:nokey=1"
                        << path);
        probe.waitForFinished();

        QString durationStr = probe.readAllStandardOutput().trimmed();
        double totalSeconds = durationStr.toDouble();

        if (totalSeconds <= 10)
            totalSeconds = 3;

        // 2. 随机 5 个时间点
        QVector<int> timePoints;
        int shotCount = 5;
        double segment = totalSeconds / shotCount;

        for (int i = 0; i < shotCount; i++) {
            int minT = static_cast<int>(i * segment);
            int maxT = static_cast<int>((i + 1) * segment);
            int range = maxT - minT;
            if (range <= 0) range = 1;
            int t = minT + QRandomGenerator::global()->bounded(range);
            if (t < 2) t = 2;
            if (t > totalSeconds - 1) t = static_cast<int>(totalSeconds) - 1;

            timePoints.append(t);
        }

        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

        // 3. 封面：10% 处
        int coverTime = static_cast<int>(totalSeconds * 0.10);
        if (coverTime < 5) coverTime = 5;

        QString coverShot = tempPath + "/cover_" + QFileInfo(path).fileName() + ".jpg";
        executeFFmpeg(path, coverTime, coverShot);

        QMetaObject::invokeMethod(this, [this, path, coverShot]() {
                // 如果期间切换了视频，就不更新旧视频的截图
                if (path != currentVideoPath)
                    return;

                QPixmap p(coverShot);
                if (!p.isNull()) {
                    coverLabel->setPixmap(
                        p.scaled(coverLabel->size(),
                                 Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation));
                }
            }, Qt::QueuedConnection);

        // 4. 详情预览图
        for (int i = 0; i < timePoints.size(); i++) {
            int t = timePoints[i];
            QString shotPath = tempPath +
                               QString("/shot_%1_%2_%3.jpg")
                                   .arg(QFileInfo(path).fileName())
                                   .arg(i)
                                   .arg(t);

            executeFFmpeg(path, t, shotPath);

            QMetaObject::invokeMethod(
                this,
                [this, path, i, shotPath]() {
                    if (path != currentVideoPath)
                        return;

                    // 保存生成好的文件路径到列表
                    if (i < m_screenshotPaths.size()) {
                        m_screenshotPaths[i] = shotPath;
                    }

                    if (i < screenshotLabels.size()) {
                        QPixmap p(shotPath);
                        if (!p.isNull()) {
                            screenshotLabels[i]->setPixmap(
                                p.scaled(screenshotLabels[i]->size(),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
                        }
                    }
                }, Qt::QueuedConnection);
        }
    });

    shotWatcher.setFuture(future);
}

void VideoDetailWidget::executeFFmpeg(const QString &input,
                                      int seconds,
                                      const QString &output)
{
    QStringList args;
    args << "-ss" << QString::number(seconds)
         << "-i" << input
         << "-frames:v" << "1"
         << "-q:v" << "3"
         << output << "-y";

    runFfmpegBlocking(args);
}

void VideoDetailWidget::playCurrentVideo() {
    if (currentVideoPath.isEmpty()) return;

    // 1. 获取当前程序运行目录 (exe 所在位置)
    QString appDir = QCoreApplication::applicationDirPath();

    // 2. 定义 PotPlayer 可能存在的路径列表
    // 优先检查 PotPlayer 子文件夹，优先使用 64 位版本
    QStringList potentialPaths = {
        appDir + "/PotPlayer/PotPlayerMini64.exe",
        appDir + "/PotPlayer/PotPlayerMini.exe",
        appDir + "/PotPlayerMini64.exe",
        appDir + "/PotPlayerMini.exe"
    };

    QString potPlayerExe;
    // 遍历查找是否存在
    for (const QString &path : potentialPaths) {
        if (QFileInfo::exists(path)) {
            potPlayerExe = path;
            break;
        }
    }

    // 3. 启动逻辑
    if (!potPlayerExe.isEmpty()) {
        // 方案 A: 找到了自带的 PotPlayer
        // 使用 startDetached 确保播放器独立运行，不会阻塞主界面
        QProcess::startDetached(potPlayerExe, QStringList() << currentVideoPath);
    } else {
        // 方案 B: 未找到，使用系统默认关联的播放器
        QDesktopServices::openUrl(QUrl::fromLocalFile(currentVideoPath));
    }
}
