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

VideoDetailWidget::VideoDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #000000; color: white;");

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

    titleLabel = new QLabel("Title", this);
    titleLabel->setStyleSheet(
        "font-size: 26px; font-weight: bold; color: white;");
    titleLabel->setWordWrap(true);
    infoRightLayout->addWidget(titleLabel);

    infoLabel = new QLabel("--", this);
    infoLabel->setStyleSheet(
        "font-size: 14px; color: #aaa; margin-top: 5px;");
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
    connect(playBtn, &QPushButton::clicked,
            this, &VideoDetailWidget::playCurrentVideo);
    infoRightLayout->addWidget(playBtn);

    headerInfoLayout->addLayout(infoRightLayout);
    contentLayout->addLayout(headerInfoLayout);

    // 截图区域标题
    QLabel *secTitle = new QLabel("精彩预览", this);
    secTitle->setStyleSheet(
        "font-size: 18px; font-weight: bold; color: #ddd; margin-top: 20px;");
    contentLayout->addWidget(secTitle);

    // 截图数组
    QHBoxLayout *shotsArrayLayout = new QHBoxLayout();
    shotsArrayLayout->setSpacing(15);

    for (int i = 0; i < 5; i++) {
        QLabel *shot = new QLabel(this);
        shot->setFixedSize(192, 108);
        shot->setStyleSheet(
            "background-color: #111; border-radius: 4px; border: 1px solid #333;");
        shot->setAlignment(Qt::AlignCenter);
        shot->setText(QString("#%1").arg(i + 1));
        screenshotLabels.append(shot);
        shotsArrayLayout->addWidget(shot);
    }
    shotsArrayLayout->addStretch();
    contentLayout->addLayout(shotsArrayLayout);

    contentLayout->addStretch();
    mainLayout->addWidget(contentWidget);

    connect(&shotWatcher, &QFutureWatcher<void>::finished,
            this, []() {});
}

void VideoDetailWidget::setVideoPath(const QString &path) {
    currentVideoPath = path;
    QFileInfo info(path);
    titleLabel->setText(info.fileName());
    infoLabel->setText(QString("格式: %1 | 大小: %2 MB")
                           .arg(info.suffix().toUpper())
                           .arg(info.size() / 1024.0 / 1024.0, 0, 'f', 1));

    coverLabel->setText("Loading...");
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
    if (shotWatcher.isRunning())
        return;

    auto future = QtConcurrent::run([this, path = currentVideoPath]() {
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

        // 容错：太短用 3 秒
        if (totalSeconds <= 10) totalSeconds = 3;

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

        QString tempPath =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation);

        // 3. 封面：10% 处
        int coverTime = static_cast<int>(totalSeconds * 0.10);
        if (coverTime < 5) coverTime = 5;

        QString coverShot =
            tempPath + "/cover_" + QFileInfo(path).fileName() + ".jpg";
        executeFFmpeg(path, coverTime, coverShot);

        QMetaObject::invokeMethod(
            this, [this, coverShot]() {
                QPixmap p(coverShot);
                if (!p.isNull()) {
                    coverLabel->setPixmap(
                        p.scaled(coverLabel->size(),
                                 Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation));
                }
            });

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
                this, [this, i, shotPath]() {
                    if (i < screenshotLabels.size()) {
                        QPixmap p(shotPath);
                        if (!p.isNull()) {
                            screenshotLabels[i]->setPixmap(
                                p.scaled(screenshotLabels[i]->size(),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
                        }
                    }
                });
        }
    });

    shotWatcher.setFuture(future);
}

void VideoDetailWidget::executeFFmpeg(const QString &input,
                                      int seconds,
                                      const QString &output) {
    QProcess::execute(ffmpegExecutablePath(),
                      QStringList()
                          << "-ss" << QString::number(seconds)
                          << "-i" << input
                          << "-frames:v" << "1"
                          << "-q:v" << "3"
                          << output << "-y");
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
