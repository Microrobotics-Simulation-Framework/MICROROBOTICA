#include "panels/scene_hierarchy_panel.h"

#include "scene/scene_manager.h"
#include "scene/prim_selection.h"

#include <unordered_map>

namespace microbotica::panels {

SceneHierarchyPanel::SceneHierarchyPanel(scene::SceneManager& sceneMgr,
                                           scene::PrimSelection& selection,
                                           QWidget* parent)
    : QDockWidget(tr("Scene Hierarchy"), parent)
    , sceneMgr_(sceneMgr)
    , selection_(selection)
{
    MBCA_EXPERIMENTAL_WARN("SceneHierarchyPanel");

    setObjectName("SceneHierarchyPanel");

    treeWidget_ = new QTreeWidget(this);
    treeWidget_->setHeaderLabel(tr("Prim Path"));
    treeWidget_->setColumnCount(1);
    setWidget(treeWidget_);

    connect(&sceneMgr_, &scene::SceneManager::sceneLoaded,
            this, &SceneHierarchyPanel::onSceneLoaded);
    connect(&sceneMgr_, &scene::SceneManager::sceneClosed,
            this, &SceneHierarchyPanel::onSceneClosed);
    connect(treeWidget_, &QTreeWidget::itemClicked,
            this, &SceneHierarchyPanel::onTreeItemClicked);
}

void SceneHierarchyPanel::onSceneLoaded()
{
    treeWidget_->clear();

    // Build a proper parent-child tree from flat prim path strings.
    // Paths are like "/World", "/World/Actors", "/World/Actors/UMR".
    // We parse these into a hierarchy using the "/" separator.
    std::unordered_map<std::string, QTreeWidgetItem*> pathToItem;

    const auto paths = sceneMgr_.primPaths();
    for (const auto& path : paths) {
        if (path.empty() || path == "/") continue;

        // Find the parent path: "/World/Actors/UMR" → "/World/Actors"
        auto lastSlash = path.rfind('/');
        std::string parentPath = (lastSlash > 0) ? path.substr(0, lastSlash) : "";
        std::string name = path.substr(lastSlash + 1);

        QTreeWidgetItem* item = nullptr;

        auto parentIt = pathToItem.find(parentPath);
        if (parentIt != pathToItem.end()) {
            // Parent exists — add as child
            item = new QTreeWidgetItem(parentIt->second);
        } else {
            // No parent found — add to root
            item = new QTreeWidgetItem(treeWidget_);
        }

        item->setText(0, QString::fromStdString(name));
        item->setData(0, Qt::UserRole, QString::fromStdString(path));
        item->setToolTip(0, QString::fromStdString(path));
        pathToItem[path] = item;
    }

    treeWidget_->expandAll();
}

void SceneHierarchyPanel::onSceneClosed()
{
    treeWidget_->clear();
}

void SceneHierarchyPanel::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;

    const std::string path = item->data(0, Qt::UserRole).toString().toStdString();
    selection_.select(path);
    primSelected(path);
}

} // namespace microbotica::panels
