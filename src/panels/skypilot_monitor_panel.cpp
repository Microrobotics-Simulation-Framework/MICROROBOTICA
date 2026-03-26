#include "panels/skypilot_monitor_panel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QProcess>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace microbotica::panels {

SkyPilotMonitorPanel::SkyPilotMonitorPanel(const std::string& scripts_dir,
                                             QWidget* parent)
    : QDockWidget(tr("SkyPilot Clusters"), parent)
    , scriptsDir_(scripts_dir)
{
    setObjectName("SkyPilotMonitorPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);

    // Toolbar
    auto* refreshBtn = new QPushButton(tr("Refresh"), container);
    connect(refreshBtn, &QPushButton::clicked, this, &SkyPilotMonitorPanel::refresh);
    layout->addWidget(refreshBtn);

    // Table
    model_ = new QStandardItemModel(0, 5, this);
    model_->setHorizontalHeaderLabels({
        tr("Name"), tr("Status"), tr("IP"), tr("Cloud"), tr("Cost/hr")
    });

    table_ = new QTableView(container);
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->hide();

    connect(table_, &QTableView::doubleClicked,
            this, &SkyPilotMonitorPanel::onRowDoubleClicked);

    layout->addWidget(table_);
    setWidget(container);

    // Auto-refresh timer (30s)
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(30000);
    connect(refreshTimer_, &QTimer::timeout, this, &SkyPilotMonitorPanel::refresh);
    refreshTimer_->start();
}

void SkyPilotMonitorPanel::refresh() {
    QProcess proc;
    proc.start("python3", {
        QString::fromStdString(scriptsDir_ + "/sky_resolve.py"),
        "--list"
    });

    if (!proc.waitForFinished(10000)) {
        spdlog::debug("SkyPilotMonitorPanel: sky_resolve.py timed out");
        return;
    }

    if (proc.exitCode() != 0) {
        spdlog::debug("SkyPilotMonitorPanel: sky_resolve.py failed (exit {})",
                       proc.exitCode());
        return;
    }

    std::string output = proc.readAllStandardOutput().toStdString();
    if (!output.empty()) {
        populateFromJson(output);
    }
}

void SkyPilotMonitorPanel::populateFromJson(const std::string& json_str) {
    try {
        auto clusters = nlohmann::json::parse(json_str);
        if (!clusters.is_array()) return;

        model_->removeRows(0, model_->rowCount());

        for (const auto& c : clusters) {
            int row = model_->rowCount();
            model_->insertRow(row);
            model_->setItem(row, 0, new QStandardItem(
                QString::fromStdString(c.value("name", ""))));
            model_->setItem(row, 1, new QStandardItem(
                QString::fromStdString(c.value("status", ""))));
            model_->setItem(row, 2, new QStandardItem(
                QString::fromStdString(c.value("ip", ""))));
            model_->setItem(row, 3, new QStandardItem(
                QString::fromStdString(c.value("cloud", ""))));

            auto cost = c.value("hourly_cost", 0.0);
            model_->setItem(row, 4, new QStandardItem(
                QString("$%1").arg(cost, 0, 'f', 2)));
        }
    } catch (const nlohmann::json::exception& e) {
        spdlog::debug("SkyPilotMonitorPanel: JSON parse error: {}", e.what());
    }
}

void SkyPilotMonitorPanel::onRowDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    auto* item = model_->item(index.row(), 0);
    if (item) {
        Q_EMIT connectRequested(item->text());
    }
}

} // namespace microbotica::panels
