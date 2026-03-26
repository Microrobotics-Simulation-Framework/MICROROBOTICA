#pragma once

#include <QDockWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QLabel>
#include <string>

namespace microbotica::panels {

/// Project browser dock panel.
///
/// QTreeView + QFileSystemModel rooted at the experiment directory.
/// Provides contextual awareness: which params.py is loaded, whether
/// graph.json has been generated, whether experiment.json is current.
class ProjectBrowserPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ProjectBrowserPanel(QWidget* parent = nullptr);

    /// Set the root directory to the given experiment path.
    void setExperimentDir(const std::string& dir);

Q_SIGNALS:
    void fileDoubleClicked(const QString& path);

private:
    QTreeView* tree_ = nullptr;
    QFileSystemModel* fsModel_ = nullptr;
    QLabel* rootLabel_ = nullptr;
};

} // namespace microbotica::panels
