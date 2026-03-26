#pragma once

#include <QApplication>
#include <QPalette>
#include <QString>

namespace microbotica::app {

/// Manages application theme (Light/Dark/System).
///
/// Uses Fusion style with custom palettes. System mode detects the
/// OS preference at startup. Light and Dark modes force a specific palette.
/// The OpenGL viewport stays dark regardless of theme (standard for 3D apps).
class ThemeManager {
public:
    /// Apply the theme. Call at startup and when the user changes the setting.
    /// @param mode  "system", "light", or "dark"
    static void applyTheme(const QString& mode);

    /// Get the current theme mode from QSettings.
    static QString currentTheme();

private:
    static QPalette darkPalette();
    static QPalette lightPalette();
    static bool systemPrefersDark();
};

} // namespace microbotica::app
