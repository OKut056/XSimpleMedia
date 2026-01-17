#include "FfmpegUtil.h"

#include <QCoreApplication>
#include <QDir>

QString ffmpegExecutablePath()
{
    // 程序可执行文件所在目录
    const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    // Windows 下使用同目录 ffmpeg.exe
    return QDir(appDir).filePath("ffmpeg.exe");
#else
    // 非 Windows 下，约定同目录放置 "ffmpeg"
    return QDir(appDir).filePath("ffmpeg");
#endif
}
