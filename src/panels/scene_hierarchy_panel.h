#pragma once

#include <QDockWidget>
#include <QTreeWidget>
#include <string>

#include "core/stability.h"

namespace microbotica::scene {
class SceneManager;
class PrimSelection;
} // namespace microbotica::scene

namespace microbotica::panels {

/// Dock panel showing the USD scene hierarchy as a tree.
///
/// Populates from SceneManager::primPaths() when the scene loads.
/// Emits primSelected(std::string) on tree item click, forwarding
/// the selection to PrimSelection.
class SceneHierarchyPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SceneHierarchyPanel(scene::SceneManager& sceneMgr,
                                  scene::PrimSelection& selection,
                                  QWidget* parent = nullptr);
    ~SceneHierarchyPanel() override = default;

signals:
    void primSelected(const std::string& path);

private slots:
    void onSceneLoaded();
    void onSceneClosed();
    void onTreeItemClicked(QTreeWidgetItem* item, int column);

private:
    scene::SceneManager& sceneMgr_;
    scene::PrimSelection& selection_;
    QTreeWidget* treeWidget_ = nullptr;
};

} // namespace microbotica::panels
