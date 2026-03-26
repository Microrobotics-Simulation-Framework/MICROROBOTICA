#include "panels/project_browser_panel.h"
#include <QVBoxLayout>
#include <QHeaderView>

namespace microbotica::panels {

ProjectBrowserPanel::ProjectBrowserPanel(QWidget* parent)
    : QDockWidget(tr("Project"), parent)
{
    setObjectName("ProjectBrowserPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);

    rootLabel_ = new QLabel(tr("No experiment loaded"), container);
    rootLabel_->setWordWrap(true);
    layout->addWidget(rootLabel_);

    fsModel_ = new QFileSystemModel(this);
    fsModel_->setReadOnly(true);
    // Show all files but filter out hidden dirs
    fsModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    tree_ = new QTreeView(container);
    tree_->setModel(fsModel_);
    tree_->setAnimated(true);
    tree_->setIndentation(16);
    tree_->setSortingEnabled(true);
    // Hide Size, Type, Date columns — show only Name
    tree_->hideColumn(1);
    tree_->hideColumn(2);
    tree_->hideColumn(3);
    tree_->header()->hide();

    connect(tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        QString path = fsModel_->filePath(index);
        if (fsModel_->isDir(index)) return;
        Q_EMIT fileDoubleClicked(path);
    });

    layout->addWidget(tree_);
    setWidget(container);
}

void ProjectBrowserPanel::setExperimentDir(const std::string& dir) {
    QString qdir = QString::fromStdString(dir);
    rootLabel_->setText(qdir);
    fsModel_->setRootPath(qdir);
    tree_->setRootIndex(fsModel_->index(qdir));
}

} // namespace microbotica::panels
