#include "FfmpegUtil.h"

#include <QAtomicInt> // 用于线程安全的退出标志
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

static QSet<qint64> g_ffmpegPids;
static QMutex g_ffmpegMutex;
static QAtomicInt g_isQuitting(0); // 0=运行中, 1=正在退出

// 辅助函数：将资源中的 ffmpeg 释放到本地缓存目录
QString ensureFfmpegExtracted()
{
    // 获取系统的标准缓存/数据目录，例如 C:/Users/User/AppData/Local/YourApp/
    // 或者 C:/Users/User/AppData/Local/Temp/
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(cacheDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

#ifdef Q_OS_WIN
    QString fileName = "ffmpeg.exe";
#else
    QString fileName = "ffmpeg";
#endif

    QString targetPath = dir.filePath(fileName);

    // 如果文件不存在，或者大小为0（防止之前的错误），则从资源中复制
    if (!QFile::exists(targetPath) || QFile(targetPath).size() == 0) {
        // 从 Qt 资源系统 (:前缀) 复制到 目标路径
        // 这里的 :/ffmpeg.exe 对应 resources.qrc 中的 alias
        if (QFile::copy(":/ffmpeg.exe", targetPath)) {
            // 复制成功，设置可执行权限（对 Linux/Mac 重要）
            QFile::setPermissions(targetPath,
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ExeGroup |
                                      QFile::ReadOther | QFile::ExeOther);
        }
    }

    return targetPath;
}

QString ffmpegExecutablePath()
{
    // 改为使用静态变量，保证只在第一次调用时执行文件释放操作
    static QString path = ensureFfmpegExtracted();
    return path;
}

static void registerFfmpegPid(qint64 pid)
{
    if (pid <= 0)
        return;
    QMutexLocker locker(&g_ffmpegMutex);
    g_ffmpegPids.insert(pid);
}

static void unregisterFfmpegPid(qint64 pid)
{
    if (pid <= 0)
        return;
    QMutexLocker locker(&g_ffmpegMutex);
    g_ffmpegPids.remove(pid);
}

int runFfmpegBlocking(const QStringList &args)
{
    // 如果程序正在退出，直接拒绝执行，防止产生僵尸进程
    if (g_isQuitting.loadAcquire()) {
        return -1;
    }

    QProcess proc;
    proc.setProgram(ffmpegExecutablePath());
    proc.setArguments(args);

    proc.start();
    if (!proc.waitForStarted()) {
        return -1;
    }

    const qint64 pid = proc.processId();
    registerFfmpegPid(pid);

    // 阻塞等待 ffmpeg 退出
    proc.waitForFinished(-1);

    unregisterFfmpegPid(pid);
    return proc.exitCode();
}

static void killPid(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h) {
        TerminateProcess(h, 1);
        CloseHandle(h);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif
}

void killAllFfmpegProcesses()
{
    // 先设置标志位，通知所有线程禁止启动新进程
    g_isQuitting.storeRelease(1);

    QSet<qint64> pidsCopy;
    {
        QMutexLocker locker(&g_ffmpegMutex);
        pidsCopy = g_ffmpegPids;
        // 直接清空集合，避免重复 kill
        g_ffmpegPids.clear();
    }

    for (qint64 pid : pidsCopy) {
        killPid(pid);
    }
}
