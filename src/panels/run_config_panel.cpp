#include "panels/run_config_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QPushButton>

namespace microbotica::panels {

RunConfigPanel::RunConfigPanel(QWidget* parent)
    : QDockWidget(tr("Run Configuration"), parent)
{
    setObjectName("RunConfigPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    // Mode selector
    auto* form = new QFormLayout;
    modeCombo_ = new QComboBox(container);
    modeCombo_->addItem(tr("Local"), static_cast<int>(connection::ConnectionMode::Local));
    modeCombo_->addItem(tr("Manual endpoint"), static_cast<int>(connection::ConnectionMode::Manual));
    modeCombo_->addItem(tr("Cloud (SkyPilot)"), static_cast<int>(connection::ConnectionMode::Cloud));
    form->addRow(tr("Mode:"), modeCombo_);

    // Experiment directory
    auto* dirLayout = new QHBoxLayout;
    experimentDirEdit_ = new QLineEdit(container);
    experimentDirEdit_->setPlaceholderText(tr("Select experiment directory..."));
    dirLayout->addWidget(experimentDirEdit_);

    auto* browseBtn = new QPushButton(tr("..."), container);
    browseBtn->setFixedWidth(30);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Experiment Directory"),
            experimentDirEdit_->text());
        if (!dir.isEmpty()) {
            experimentDirEdit_->setText(dir);
            Q_EMIT experimentDirChanged(dir);
        }
    });
    dirLayout->addWidget(browseBtn);
    form->addRow(tr("Experiment:"), dirLayout);

    layout->addLayout(form);

    // Launch/Stop buttons
    auto* buttonLayout = new QHBoxLayout;
    launchButton_ = new QPushButton(tr("Launch"), container);
    launchButton_->setToolTip(tr("Start MIME simulation"));
    connect(launchButton_, &QPushButton::clicked, this, &RunConfigPanel::launchRequested);
    buttonLayout->addWidget(launchButton_);

    stopButton_ = new QPushButton(tr("Stop"), container);
    stopButton_->setEnabled(false);
    stopButton_->setToolTip(tr("Stop MIME simulation"));
    connect(stopButton_, &QPushButton::clicked, this, &RunConfigPanel::stopRequested);
    buttonLayout->addWidget(stopButton_);
    layout->addLayout(buttonLayout);

    // Connection status
    statusLabel_ = new QLabel(tr("Status: Idle"), container);
    layout->addWidget(statusLabel_);

    layout->addStretch();
    setWidget(container);
}

connection::ConnectionMode RunConfigPanel::selectedMode() const {
    return static_cast<connection::ConnectionMode>(modeCombo_->currentData().toInt());
}

std::string RunConfigPanel::experimentDir() const {
    return experimentDirEdit_->text().toStdString();
}

void RunConfigPanel::setConnectionStatus(const QString& status) {
    statusLabel_->setText(tr("Status: %1").arg(status));
}

void RunConfigPanel::setLaunchEnabled(bool enabled) {
    launchButton_->setEnabled(enabled);
    stopButton_->setEnabled(!enabled);
}

} // namespace microbotica::panels
