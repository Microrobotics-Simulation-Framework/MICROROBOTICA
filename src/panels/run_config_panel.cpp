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

    // Stream format selector (MIME v0.2 fit-up §8). "Auto" leaves the
    // runner at its default; JSON/Binary set MIME_STREAM_FORMAT on the
    // spawned runner. MICROROBOTICA decodes whichever the handshake reports.
    streamFormatCombo_ = new QComboBox(container);
    streamFormatCombo_->addItem(tr("Auto (runner default)"), QString());
    streamFormatCombo_->addItem(tr("JSON"), QStringLiteral("json"));
    streamFormatCombo_->addItem(tr("Binary"), QStringLiteral("binary"));
    streamFormatCombo_->setToolTip(
        tr("ResultFrame wire format the spawned MIME runner streams.\n"
           "Binary is compact (~7x smaller); MICROROBOTICA negotiates and\n"
           "decodes whichever the runner declares."));
    form->addRow(tr("Stream format:"), streamFormatCombo_);

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

void RunConfigPanel::setExperimentDir(const std::string& path) {
    // experimentDirChanged is only emitted from the browse button — calling
    // QLineEdit::setText here triggers Qt's built-in textChanged but not
    // our custom signal, so this does not re-enter initExperiment.
    experimentDirEdit_->setText(QString::fromStdString(path));
}

std::string RunConfigPanel::streamFormat() const {
    return streamFormatCombo_->currentData().toString().toStdString();
}

void RunConfigPanel::setConnectionStatus(const QString& status) {
    statusLabel_->setText(tr("Status: %1").arg(status));
}

void RunConfigPanel::setLaunchEnabled(bool enabled) {
    launchButton_->setEnabled(enabled);
    stopButton_->setEnabled(!enabled);
}

} // namespace microbotica::panels
