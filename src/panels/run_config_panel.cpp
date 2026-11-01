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
    // Cloud (SkyPilot) is set aside: MIME 0.3's remote answer is a
    // `mime serve` reached through an SSH tunnel (Manual endpoint), and
    // pixel streaming may return with MADDENING 0.5.0. CloudJobLauncher and
    // its tests stay in the tree, unwired.
    modeCombo_->setToolTip(
        tr("Local: run the opened saved experiment here.\n"
           "Manual endpoint: connect to a session already serving, for "
           "example a remote one through an SSH tunnel."));
    form->addRow(tr("Mode:"), modeCombo_);

    // Experiment directory
    auto* dirLayout = new QHBoxLayout;
    experimentDirEdit_ = new QLineEdit(container);
    experimentDirEdit_->setPlaceholderText(tr("A MIME 0.3 saved experiment folder..."));
    dirLayout->addWidget(experimentDirEdit_);

    auto* browseBtn = new QPushButton(tr("..."), container);
    browseBtn->setFixedWidth(30);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select a Saved Experiment"),
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

void RunConfigPanel::setExperimentDir(const std::string& path) {
    // experimentDirChanged is only emitted from the browse button — calling
    // QLineEdit::setText here triggers Qt's built-in textChanged but not
    // our custom signal, so this does not re-enter initExperiment.
    experimentDirEdit_->setText(QString::fromStdString(path));
}

void RunConfigPanel::setConnectionStatus(const QString& status) {
    statusLabel_->setText(tr("Status: %1").arg(status));
}

void RunConfigPanel::setLaunchEnabled(bool enabled) {
    launchButton_->setEnabled(enabled);
    stopButton_->setEnabled(!enabled);
}

} // namespace microbotica::panels
