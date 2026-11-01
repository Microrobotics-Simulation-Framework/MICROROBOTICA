#include "app/application.h"
#include "app/main_window.h"

#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char* argv[])
{
    microbotica::app::Application app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("MICROROBOTICA: the simulator UI for MIME"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("path"),
        QStringLiteral("A MIME 0.3 saved experiment folder, or a USD file (a run folder's recording.usdc), to open."));
    const QCommandLineOption launch(QStringLiteral("launch"),
        QStringLiteral("Run the opened saved experiment live at once (mime serve)."));
    const QCommandLineOption quitAfter(QStringLiteral("quit-after"),
        QStringLiteral("Close the window, cleanly, after <seconds> (scripted runs)."), QStringLiteral("seconds"));
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
        QStringLiteral("With --quit-after: save a picture of the window to <png> just before closing."),
        QStringLiteral("png"));
    parser.addOption(launch);
    parser.addOption(quitAfter);
    parser.addOption(screenshot);
    parser.process(app);

    microbotica::app::MainWindow mainWindow;
    mainWindow.show();

    const QStringList paths = parser.positionalArguments();
    if (!paths.isEmpty()) {
        const QString path = paths.first();
        const bool startNow = parser.isSet(launch);
        QTimer::singleShot(0, &mainWindow, [&mainWindow, path, startNow]() {
            if (mainWindow.openPath(path) && startNow) {
                mainWindow.launch();
            }
        });
    }
    if (parser.isSet(quitAfter)) {
        const int ms = static_cast<int>(parser.value(quitAfter).toDouble() * 1000.0);
        const QString picture = parser.value(screenshot);
        QTimer::singleShot(ms, &mainWindow, [&mainWindow, picture]() {
            if (!picture.isEmpty()) {
                mainWindow.grab().save(picture);
            }
            mainWindow.close();
        });
    }

    return app.exec();
}
