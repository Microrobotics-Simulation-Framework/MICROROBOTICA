#include "panels/script_editor_panel.h"
#include "panels/python_highlighter.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>

namespace microbotica::panels {

ScriptEditorPanel::ScriptEditorPanel(QWidget* parent)
    : QDockWidget(tr("Script Editor"), parent)
{
    setObjectName("ScriptEditorPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar
    auto* toolbar = new QToolBar(container);
    auto* saveAction = toolbar->addAction(tr("Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &ScriptEditorPanel::saveCurrent);

    auto* saveAllAction = toolbar->addAction(tr("Save All"));
    saveAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveAllAction, &QAction::triggered, this, &ScriptEditorPanel::saveAll);

    layout->addWidget(toolbar);

    // Tab widget
    tabs_ = new QTabWidget(container);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    connect(tabs_, &QTabWidget::tabCloseRequested,
            this, &ScriptEditorPanel::onTabCloseRequested);
    layout->addWidget(tabs_);

    // File system watcher
    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged,
            this, &ScriptEditorPanel::onFileChanged);

    setWidget(container);
}

void ScriptEditorPanel::openFile(const std::string& path) {
    // Check if already open
    int existingTab = findTab(path);
    if (existingTab >= 0) {
        tabs_->setCurrentIndex(existingTab);
        return;
    }

    // Read file
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString content = QTextStream(&file).readAll();
    file.close();

    // Create editor
    auto* editor = new QPlainTextEdit;
    editor->setFont(QFont("Monospace", 10));
    editor->setTabStopDistance(
        QFontMetricsF(editor->font()).horizontalAdvance(' ') * 4);
    editor->setPlainText(content);

    // Syntax highlighting for .py files
    if (QString::fromStdString(path).endsWith(".py")) {
        new PythonHighlighter(editor->document());
    }

    connect(editor->document(), &QTextDocument::modificationChanged,
            this, &ScriptEditorPanel::onTextModified);

    // Add tab
    QFileInfo fi(QString::fromStdString(path));
    int idx = tabs_->addTab(editor, fi.fileName());
    tabs_->setCurrentIndex(idx);

    TabInfo info;
    info.editor = editor;
    info.filePath = path;
    info.modified = false;
    tabInfos_.push_back(std::move(info));

    // Watch for external changes
    watcher_->addPath(QString::fromStdString(path));
}

void ScriptEditorPanel::saveCurrent() {
    int idx = tabs_->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(tabInfos_.size())) return;

    auto& info = tabInfos_[static_cast<size_t>(idx)];

    // Temporarily stop watching (our own write would trigger fileChanged)
    watcher_->removePath(QString::fromStdString(info.filePath));

    QFile file(QString::fromStdString(info.filePath));
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << info.editor->toPlainText();
        file.close();

        info.modified = false;
        info.editor->document()->setModified(false);
        updateTabTitle(idx);

        Q_EMIT fileSaved(QString::fromStdString(info.filePath));
    }

    // Resume watching
    watcher_->addPath(QString::fromStdString(info.filePath));
}

void ScriptEditorPanel::saveAll() {
    int current = tabs_->currentIndex();
    for (int i = 0; i < static_cast<int>(tabInfos_.size()); ++i) {
        if (tabInfos_[static_cast<size_t>(i)].modified) {
            tabs_->setCurrentIndex(i);
            saveCurrent();
        }
    }
    tabs_->setCurrentIndex(current);
}

void ScriptEditorPanel::onTabCloseRequested(int index) {
    if (index < 0 || index >= static_cast<int>(tabInfos_.size())) return;

    auto& info = tabInfos_[static_cast<size_t>(index)];
    if (info.modified) {
        auto result = QMessageBox::question(this, tr("Unsaved Changes"),
            tr("Save changes to %1?").arg(
                QFileInfo(QString::fromStdString(info.filePath)).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (result == QMessageBox::Cancel) return;
        if (result == QMessageBox::Save) {
            tabs_->setCurrentIndex(index);
            saveCurrent();
        }
    }

    watcher_->removePath(QString::fromStdString(info.filePath));
    tabs_->removeTab(index);
    tabInfos_.erase(tabInfos_.begin() + index);
}

void ScriptEditorPanel::onFileChanged(const QString& path) {
    std::string spath = path.toStdString();
    int idx = findTab(spath);
    if (idx < 0) return;

    auto& info = tabInfos_[static_cast<size_t>(idx)];

    // Read new content
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString newContent = QTextStream(&file).readAll();
    file.close();

    if (info.modified) {
        // User has unsaved changes — ask what to do
        auto result = QMessageBox::question(this, tr("External Change"),
            tr("%1 was modified externally.\n\nReload and lose your changes?")
                .arg(QFileInfo(path).fileName()),
            QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::No) return;
    }

    // Reload content
    int cursor = info.editor->textCursor().position();
    info.editor->setPlainText(newContent);

    // Restore cursor position (clamped to new length)
    QTextCursor tc = info.editor->textCursor();
    tc.setPosition(qMin(cursor, info.editor->document()->characterCount() - 1));
    info.editor->setTextCursor(tc);

    info.modified = false;
    info.editor->document()->setModified(false);
    updateTabTitle(idx);

    // Re-add to watcher (some OS remove the watch after a change)
    watcher_->addPath(path);
}

void ScriptEditorPanel::onTextModified() {
    int idx = tabs_->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(tabInfos_.size())) return;

    auto& info = tabInfos_[static_cast<size_t>(idx)];
    info.modified = info.editor->document()->isModified();
    updateTabTitle(idx);
}

int ScriptEditorPanel::findTab(const std::string& path) const {
    for (size_t i = 0; i < tabInfos_.size(); ++i) {
        if (tabInfos_[i].filePath == path) return static_cast<int>(i);
    }
    return -1;
}

void ScriptEditorPanel::updateTabTitle(int index) {
    if (index < 0 || index >= static_cast<int>(tabInfos_.size())) return;
    const auto& info = tabInfos_[static_cast<size_t>(index)];
    QFileInfo fi(QString::fromStdString(info.filePath));
    QString title = fi.fileName();
    if (info.modified) title += " *";
    tabs_->setTabText(index, title);
}

} // namespace microbotica::panels
