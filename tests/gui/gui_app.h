#pragma once

// The one QApplication of the GUI tests (microbotica_gui_tests). On the X display DISPLAY names, through Qt's xcb
// platform and its GLX path, when there is one: the viewport then gets the OpenGL context the app gets on a desktop,
// which Qt's offscreen platform cannot give (it has no OpenGL at all). Without a display it is offscreen, and only the
// tests that need no OpenGL run; the others are skipped. QT_QPA_PLATFORM, when set, is left as it is.

#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QString>

namespace microbotica::testing {

inline QApplication& guiApp() {
    static int argc = 1;
    static char name[] = "microbotica_gui_tests";
    static char* argv[] = {name, nullptr};
    // Never destroyed: Qt's and OpenUSD's singletons outlive the tests in any order.
    static QApplication* app = [] {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsEmpty("DISPLAY") ? QByteArray("offscreen")
                                                                               : QByteArray("xcb"));
        }
        return new QApplication(argc, argv);
    }();
    return *app;
}

/// True when the tests run on an X display (Qt's xcb platform), where OpenGL contexts are the desktop's.
inline bool onXDisplay() {
    guiApp();
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

} // namespace microbotica::testing
