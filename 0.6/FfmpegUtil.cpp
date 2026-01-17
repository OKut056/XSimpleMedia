#include "FfmpegUtil.h"

#include <QAtomicInt> // 用于线程安全的退出标志
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

static QSet<qint64> g_ffmpegPids;
static QMutex g_ffmpegMutex;
static QAtomicInt g_isQuitting(0); // 0=运行中, 1=正在退出

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
