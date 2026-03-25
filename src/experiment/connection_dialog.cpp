#include "experiment/connection_dialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>

namespace microbotica::experiment {

ConnectionChoice showConnectionDialog(QWidget* parent, bool has_cloud_config) {
    QMessageBox box(parent);
    box.setWindowTitle("No MIME Process Detected");
    box.setText("No MIME physics backend was found on localhost.\n"
                "How would you like to connect?");
    box.setIcon(QMessageBox::Question);

    auto* launch_btn = box.addButton("Launch locally", QMessageBox::AcceptRole);
    auto* manual_btn = box.addButton("Enter endpoint", QMessageBox::ActionRole);

    QPushButton* cloud_btn = nullptr;
    if (has_cloud_config) {
        cloud_btn = box.addButton("Launch on cloud", QMessageBox::ActionRole);
    }

    box.addButton(QMessageBox::Cancel);

    box.exec();

    auto* clicked = box.clickedButton();

    if (clicked == launch_btn) {
        return {ConnectionChoice::LaunchLocal, {}};
    }

    if (clicked == manual_btn) {
        bool ok = false;
        QString endpoint = QInputDialog::getText(
            parent,
            "Manual Endpoint",
            "Enter MIME ZMQ endpoint (e.g., tcp://192.168.1.5:5556):",
            QLineEdit::Normal,
            "tcp://localhost:5556",
            &ok);

        if (ok && !endpoint.isEmpty()) {
            return {ConnectionChoice::ManualEndpoint, endpoint.toStdString()};
        }
        return {ConnectionChoice::Cancelled, {}};
    }

    if (cloud_btn && clicked == cloud_btn) {
        return {ConnectionChoice::LaunchCloud, {}};
    }

    return {ConnectionChoice::Cancelled, {}};
}

} // namespace microbotica::experiment
