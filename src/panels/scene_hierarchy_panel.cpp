#include "panels/scene_hierarchy_panel.h"

#include "scene/scene_manager.h"
#include "scene/prim_selection.h"

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
    connect(treeWidget_, &QTreeWidget::itemClicked,
            this, &SceneHierarchyPanel::onTreeItemClicked);
}

void SceneHierarchyPanel::onSceneLoaded()
{
    treeWidget_->clear();

    const auto paths = sceneMgr_.primPaths();
    for (const auto& path : paths) {
        auto* item = new QTreeWidgetItem(treeWidget_);
        item->setText(0, QString::fromStdString(path));
        item->setData(0, Qt::UserRole, QString::fromStdString(path));
    }
}

void SceneHierarchyPanel::onTreeItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;

    const std::string path = item->data(0, Qt::UserRole).toString().toStdString();
    selection_.select(path);
    primSelected(path);
}

} // namespace microbotica::panels
