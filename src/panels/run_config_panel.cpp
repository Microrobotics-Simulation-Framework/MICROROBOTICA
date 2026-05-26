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

    // Target publish rate for the runner. 0 means "use the runner's
    // default" (60 fps as of MIME v0.2). Useful for stress-testing the
    // worker-side skip-to-latest path on complex scenes.
    publishFpsSpin_ = new QSpinBox(container);
    publishFpsSpin_->setRange(0, 1000);
    publishFpsSpin_->setValue(0);
    publishFpsSpin_->setSpecialValueText(tr("Runner default"));
    publishFpsSpin_->setSuffix(tr(" fps"));
    publishFpsSpin_->setToolTip(
        tr("Target on-the-wire publish rate for the spawned MIME runner.\n"
           "0 = leave at runner default (60 fps). Higher values stress the\n"
           "worker / scene-apply pipeline; lower values reduce CPU."));
    form->addRow(tr("Publish rate:"), publishFpsSpin_);

    // Cloud-mode fields — hidden until the user picks Cloud.
    clusterNameEdit_ = new QLineEdit(container);
    clusterNameEdit_->setPlaceholderText(tr("ar4-test"));
    clusterNameEdit_->setToolTip(
        tr("SkyPilot cluster name. Used to launch via `sky launch --cluster`\n"
           "and to resolve the cluster IP for the SSH tunnel."));
    clusterNameLabel_ = new QLabel(tr("Cluster name:"), container);
    form->addRow(clusterNameLabel_, clusterNameEdit_);

    jobConfigEdit_ = new QLineEdit(container);
    jobConfigEdit_->setPlaceholderText(tr("jobs/local_ssh.yaml"));
    jobConfigEdit_->setToolTip(
        tr("Path to the SkyPilot job YAML, relative to the experiment dir.\n"
           "If empty, `cloud.job_config` from experiment.yaml is used."));
    jobConfigLabel_ = new QLabel(tr("Job config:"), container);
    form->addRow(jobConfigLabel_, jobConfigEdit_);

    stopClusterCheck_ = new QCheckBox(
        tr("Stop cluster on disconnect (`sky down`)"), container);
    stopClusterCheck_->setChecked(true);
    stopClusterCheck_->setToolTip(
        tr("On Stop, also tear the SkyPilot cluster down. Strongly\n"
           "recommended for billed cloud providers; you can uncheck this\n"
           "when iterating against a long-lived dev cluster."));
    form->addRow(QString(), stopClusterCheck_);

    auto updateCloudFieldVisibility = [this]() {
        const bool cloud =
            modeCombo_->currentData().toInt()
            == static_cast<int>(connection::ConnectionMode::Cloud);
        clusterNameLabel_->setVisible(cloud);
        clusterNameEdit_->setVisible(cloud);
        jobConfigLabel_->setVisible(cloud);
        jobConfigEdit_->setVisible(cloud);
        stopClusterCheck_->setVisible(cloud);
    };
    updateCloudFieldVisibility();
    connect(modeCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [updateCloudFieldVisibility](int) {
        updateCloudFieldVisibility();
    });

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

int RunConfigPanel::publishFps() const {
    return publishFpsSpin_->value();
}

std::string RunConfigPanel::clusterName() const {
    return clusterNameEdit_->text().trimmed().toStdString();
}

std::string RunConfigPanel::jobConfig() const {
    return jobConfigEdit_->text().trimmed().toStdString();
}

bool RunConfigPanel::stopClusterOnDisconnect() const {
    return stopClusterCheck_->isChecked();
}

void RunConfigPanel::setConnectionStatus(const QString& status) {
    statusLabel_->setText(tr("Status: %1").arg(status));
}

void RunConfigPanel::setLaunchEnabled(bool enabled) {
    launchButton_->setEnabled(enabled);
    stopButton_->setEnabled(!enabled);
}

} // namespace microbotica::panels
