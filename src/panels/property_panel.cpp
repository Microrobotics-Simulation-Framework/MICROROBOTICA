#include "panels/property_panel.h"

#include <QHeaderView>
#include "scene/prim_selection.h"

namespace microbotica::panels {

PropertyPanel::PropertyPanel(scene::PrimSelection& selection,
                               QWidget* parent)
    : QDockWidget(tr("Properties"), parent)
    , selection_(selection)
{
    MBCA_EXPERIMENTAL_WARN("PropertyPanel");

    setObjectName("PropertyPanel");

    tableWidget_ = new QTableWidget(this);
    tableWidget_->setColumnCount(2);
    tableWidget_->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    tableWidget_->horizontalHeader()->setStretchLastSection(true);
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    setWidget(tableWidget_);

    connect(&selection_, &scene::PrimSelection::primSelected,
            this, &PropertyPanel::onPrimSelected);
}

void PropertyPanel::onPrimSelected(const std::string& path)
{
    tableWidget_->setRowCount(0);

    if (path.empty()) return;

    // Row 0: Prim path
    tableWidget_->insertRow(0);
    tableWidget_->setItem(0, 0, new QTableWidgetItem(tr("Prim Path")));
    tableWidget_->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(path)));

    // Row 1: Placeholder type
    tableWidget_->insertRow(1);
    tableWidget_->setItem(1, 0, new QTableWidgetItem(tr("Type")));
    tableWidget_->setItem(1, 1, new QTableWidgetItem(tr("(not yet resolved)")));

    // Row 2: Placeholder visibility
    tableWidget_->insertRow(2);
    tableWidget_->setItem(2, 0, new QTableWidgetItem(tr("Visibility")));
    tableWidget_->setItem(2, 1, new QTableWidgetItem(tr("inherited")));
}

} // namespace microbotica::panels
