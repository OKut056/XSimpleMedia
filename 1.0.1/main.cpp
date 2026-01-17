#include <QApplication>
#include <cstdlib>
#include "YouTubeStyleManager.h"
#include "FfmpegUtil.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        killAllFfmpegProcesses();

        // === 强制关闭 Windows 照片查看器 ===
#ifdef Q_OS_WIN
        // /F 强制终止, /IM 指定镜像名称
        // 注意：这会关闭所有打开的照片窗口！
        system("taskkill /F /IM Photos.exe >nul 2>&1");
        system("taskkill /F /IM Microsoft.Photos.exe >nul 2>&1");
#endif
    });

    YouTubeStyleManager w;
    w.show();
    return app.exec();
}
