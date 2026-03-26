#pragma once

#include <QDockWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QTimer>
#include <QPushButton>

namespace microbotica::panels {

/// SkyPilot cluster monitor panel.
///
/// Displays a table of active SkyPilot clusters populated by
/// `sky_resolve.py --list` JSON output. Refreshes every 30s via QTimer.
/// Click-to-connect on a row emits connectRequested with the cluster name.
class SkyPilotMonitorPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SkyPilotMonitorPanel(const std::string& scripts_dir,
                                   QWidget* parent = nullptr);

Q_SIGNALS:
    void connectRequested(const QString& cluster_name);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onRowDoubleClicked(const QModelIndex& index);

private:
    void populateFromJson(const std::string& json_str);

    QTableView* table_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    std::string scriptsDir_;
};

} // namespace microbotica::panels
