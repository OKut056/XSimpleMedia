#include <QApplication>
#include "YouTubeStyleManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    YouTubeStyleManager w;
    w.show();
    return app.exec();
}
