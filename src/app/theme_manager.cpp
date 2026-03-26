#include "app/theme_manager.h"
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QGuiApplication>

namespace microbotica::app {

void ThemeManager::applyTheme(const QString& mode) {
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    bool dark = false;
    if (mode == "dark") {
        dark = true;
    } else if (mode == "light") {
        dark = false;
    } else {
        // "system" — detect OS preference
        dark = systemPrefersDark();
    }

    QApplication::setPalette(dark ? darkPalette() : lightPalette());
}

QString ThemeManager::currentTheme() {
    QSettings s;
    s.beginGroup("theme");
    QString theme = s.value("mode", "system").toString();
    s.endGroup();
    return theme;
}

QPalette ThemeManager::darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Base, QColor(35, 35, 35));
    p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    p.setColor(QPalette::ToolTipBase, QColor(25, 25, 25));
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    return p;
}

QPalette ThemeManager::lightPalette() {
    // Use the default Fusion light palette
    return QApplication::style()->standardPalette();
}

bool ThemeManager::systemPrefersDark() {
    // Qt 6.5+ has QStyleHints::colorScheme().
    // For Qt 6.4 compat, check the window background lightness.
    auto palette = QGuiApplication::palette();
    auto windowColor = palette.color(QPalette::Window);
    return windowColor.lightness() < 128;
}

} // namespace microbotica::app
