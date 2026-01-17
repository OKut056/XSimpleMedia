#include <QApplication>
#include "YouTubeStyleManager.h"
#include "FfmpegUtil.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        killAllFfmpegProcesses();
    });

    YouTubeStyleManager w;
    w.show();
    return app.exec();
}
