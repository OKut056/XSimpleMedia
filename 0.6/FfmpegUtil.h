#ifndef FFMPEGUTIL_H
#define FFMPEGUTIL_H

#pragma once
#include <QString>
#include <QStringList>

// 返回 ffmpeg 可执行文件的完整路径：程序所在目录下的 ffmpeg.exe（或 ffmpeg）
QString ffmpegExecutablePath();

// 阻塞式调用 ffmpeg；内部会记录 PID，供退出时杀进程
int runFfmpegBlocking(const QStringList &args);

// 在应用退出时调用，杀掉所有仍在运行的 ffmpeg 子进程
void killAllFfmpegProcesses();

#endif // FFMPEGUTIL_H
