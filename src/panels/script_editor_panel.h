#pragma once

#include <QDockWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QFileSystemWatcher>
#include <unordered_map>
#include <string>

namespace microbotica::panels {

/// Script editor dock panel with tabbed file editing.
///
/// Opens experiment files (params.py, controller.py, setup.py) in tabs
/// with Python syntax highlighting. Monitors open files via
/// QFileSystemWatcher — if a file is changed externally (e.g., in VS Code),
/// the tab content is reloaded automatically (if unmodified in the editor)
/// or the user is prompted (if they have unsaved changes).
///
/// This is a lightweight editor for quick tweaks. For serious development,
/// users should use their preferred external editor — the file watcher
/// ensures the script editor always shows the latest version.
class ScriptEditorPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ScriptEditorPanel(QWidget* parent = nullptr);

    /// Open a file in a new tab (or switch to existing tab if already open).
    void openFile(const std::string& path);

    /// Save the current tab's content to disk.
    void saveCurrent();

    /// Save all modified tabs.
    void saveAll();

Q_SIGNALS:
    /// Emitted when a file is saved (for triggering param reload etc.)
    void fileSaved(const QString& path);

private Q_SLOTS:
    void onTabCloseRequested(int index);
    void onFileChanged(const QString& path);
    void onTextModified();

private:
    struct TabInfo {
        QPlainTextEdit* editor = nullptr;
        std::string filePath;
        bool modified = false;
    };

    int findTab(const std::string& path) const;
    void updateTabTitle(int index);

    QTabWidget* tabs_ = nullptr;
    QFileSystemWatcher* watcher_ = nullptr;
    std::vector<TabInfo> tabInfos_;
};

} // namespace microbotica::panels
