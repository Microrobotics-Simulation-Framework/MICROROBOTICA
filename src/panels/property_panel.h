#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <string>

#include "core/stability.h"

namespace microbotica::scene {
class PrimSelection;
} // namespace microbotica::scene

namespace microbotica::panels {

/// Dock panel displaying properties for the currently selected prim.
///
/// Subscribes to PrimSelection::primSelected to update the table.
/// Shows the prim path and placeholder properties for Phase 0.
class PropertyPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(scene::PrimSelection& selection,
                           QWidget* parent = nullptr);
    ~PropertyPanel() override = default;

private slots:
    void onPrimSelected(const std::string& path);

private:
    scene::PrimSelection& selection_;
    QTableWidget* tableWidget_ = nullptr;
};

} // namespace microbotica::panels
