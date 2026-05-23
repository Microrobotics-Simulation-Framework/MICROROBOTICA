#include "app/settings_dialog.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPushButton>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace microbotica::app {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    resize(500, 400);

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    createGeneralTab(tabs);
    createThemeTab(tabs);
    createCloudTab(tabs);

    layout->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    loadSettings();
}

void SettingsDialog::createGeneralTab(QTabWidget* tabs) {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    // Default experiment directory
    auto* dirLayout = new QHBoxLayout;
    defaultExperimentDir_ = new QLineEdit(page);
    dirLayout->addWidget(defaultExperimentDir_);
    auto* browseBtn = new QPushButton(tr("..."), page);
    browseBtn->setFixedWidth(30);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Default Experiment Directory"),
            defaultExperimentDir_->text());
        if (!dir.isEmpty()) defaultExperimentDir_->setText(dir);
    });
    dirLayout->addWidget(browseBtn);
    form->addRow(tr("Default experiment directory:"), dirLayout);

    // ZMQ ports
    zmqPubPort_ = new QSpinBox(page);
    zmqPubPort_->setRange(1024, 65535);
    zmqPubPort_->setValue(5556);
    form->addRow(tr("ZMQ PUB port:"), zmqPubPort_);

    zmqReqPort_ = new QSpinBox(page);
    zmqReqPort_->setRange(1024, 65535);
    zmqReqPort_->setValue(5555);
    form->addRow(tr("ZMQ REQ port:"), zmqReqPort_);

    // ── Notices: viewport overlay shown for long-running background state
    // (waiting on the MIME runner, decode errors, ...). Style is inspired
    // by noice.nvim — top-right floating cards with a severity accent.
    form->addRow(new QLabel(tr(""), page)); // spacer
    auto* noticesHeader = new QLabel(
        tr("<b>Viewport notices</b>"), page);
    form->addRow(noticesHeader);

    showNotices_ = new QCheckBox(tr("Show notices over the 3D viewport"), page);
    form->addRow(showNotices_);

    noticeOpacity_ = new QSlider(Qt::Horizontal, page);
    noticeOpacity_->setRange(20, 100);
    noticeOpacity_->setValue(85);
    noticeOpacity_->setToolTip(
        tr("Background opacity of the notice cards (20–100%)."));
    form->addRow(tr("Notice opacity:"), noticeOpacity_);

    tabs->addTab(page, tr("General"));
}

void SettingsDialog::createThemeTab(QTabWidget* tabs) {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    themeCombo_ = new QComboBox(page);
    themeCombo_->addItem(tr("System"), "system");
    themeCombo_->addItem(tr("Light"), "light");
    themeCombo_->addItem(tr("Dark"), "dark");
    form->addRow(tr("Theme:"), themeCombo_);

    auto* note = new QLabel(
        tr("Changes take effect after clicking OK."), page);
    note->setWordWrap(true);
    form->addRow(note);

    tabs->addTab(page, tr("Theme"));
}

void SettingsDialog::createCloudTab(QTabWidget* tabs) {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    // Credential file path
    auto* credLayout = new QHBoxLayout;
    credentialFilePath_ = new QLineEdit(page);
    credentialFilePath_->setPlaceholderText(
        QDir::homePath() + "/.maddening/cloud_credentials.yaml");
    credLayout->addWidget(credentialFilePath_);
    auto* browseBtn = new QPushButton(tr("..."), page);
    browseBtn->setFixedWidth(30);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(
            this, tr("Cloud Credentials File"),
            credentialFilePath_->text(),
            tr("YAML files (*.yaml *.yml);;All files (*)"));
        if (!file.isEmpty()) {
            credentialFilePath_->setText(file);
            refreshCredentialStatus();
        }
    });
    credLayout->addWidget(browseBtn);
    form->addRow(tr("Credentials file:"), credLayout);

    // Credential status
    credentialStatus_ = new QLabel(page);
    form->addRow(tr("Status:"), credentialStatus_);

    auto* refreshBtn = new QPushButton(tr("Check credentials"), page);
    connect(refreshBtn, &QPushButton::clicked, this,
            &SettingsDialog::refreshCredentialStatus);
    form->addRow(refreshBtn);

    form->addRow(new QLabel(tr(""), page)); // spacer

    // Cost policy defaults
    auto* costLabel = new QLabel(tr("<b>Default cost policy</b> (overridden per-experiment in jobs/*.yaml)"), page);
    costLabel->setWordWrap(true);
    form->addRow(costLabel);

    maxCostPerHour_ = new QDoubleSpinBox(page);
    maxCostPerHour_->setRange(0.10, 100.0);
    maxCostPerHour_->setValue(2.00);
    maxCostPerHour_->setPrefix("$");
    maxCostPerHour_->setSingleStep(0.50);
    form->addRow(tr("Max cost/hour:"), maxCostPerHour_);

    maxTotalBudget_ = new QDoubleSpinBox(page);
    maxTotalBudget_->setRange(0.50, 1000.0);
    maxTotalBudget_->setValue(5.00);
    maxTotalBudget_->setPrefix("$");
    maxTotalBudget_->setSingleStep(1.0);
    form->addRow(tr("Max total budget:"), maxTotalBudget_);

    autostopMinutes_ = new QSpinBox(page);
    autostopMinutes_->setRange(5, 1440);
    autostopMinutes_->setValue(15);
    autostopMinutes_->setSuffix(tr(" min"));
    form->addRow(tr("Autostop timeout:"), autostopMinutes_);

    // SSH key path (read-only)
    sshKeyPath_ = new QLabel(page);
    sshKeyPath_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("SSH key:"), sshKeyPath_);

    tabs->addTab(page, tr("Cloud"));
}

void SettingsDialog::loadSettings() {
    QSettings s;
    s.beginGroup("general");
    defaultExperimentDir_->setText(s.value("default_experiment_dir").toString());
    zmqPubPort_->setValue(s.value("zmq_pub_port", 5556).toInt());
    zmqReqPort_->setValue(s.value("zmq_req_port", 5555).toInt());
    s.endGroup();

    s.beginGroup("notices");
    showNotices_->setChecked(s.value("enabled", true).toBool());
    noticeOpacity_->setValue(s.value("opacity", 85).toInt());
    s.endGroup();

    s.beginGroup("theme");
    QString theme = s.value("mode", "system").toString();
    int idx = themeCombo_->findData(theme);
    if (idx >= 0) themeCombo_->setCurrentIndex(idx);
    s.endGroup();

    s.beginGroup("cloud");
    QString credPath = s.value("credentials_path",
        QDir::homePath() + "/.maddening/cloud_credentials.yaml").toString();
    credentialFilePath_->setText(credPath);
    maxCostPerHour_->setValue(s.value("max_cost_per_hour", 2.00).toDouble());
    maxTotalBudget_->setValue(s.value("max_total_budget", 5.00).toDouble());
    autostopMinutes_->setValue(s.value("autostop_minutes", 15).toInt());
    s.endGroup();

    // SSH key path — check ~/.ssh/
    QString sshDir = QDir::homePath() + "/.ssh";
    if (QFileInfo::exists(sshDir + "/sky-key")) {
        sshKeyPath_->setText(sshDir + "/sky-key (SkyPilot managed)");
    } else if (QFileInfo::exists(sshDir + "/id_rsa")) {
        sshKeyPath_->setText(sshDir + "/id_rsa");
    } else {
        sshKeyPath_->setText(tr("Not found"));
    }

    refreshCredentialStatus();
}

void SettingsDialog::saveSettings() {
    QSettings s;
    s.beginGroup("general");
    s.setValue("default_experiment_dir", defaultExperimentDir_->text());
    s.setValue("zmq_pub_port", zmqPubPort_->value());
    s.setValue("zmq_req_port", zmqReqPort_->value());
    s.endGroup();

    s.beginGroup("notices");
    s.setValue("enabled", showNotices_->isChecked());
    s.setValue("opacity", noticeOpacity_->value());
    s.endGroup();

    s.beginGroup("theme");
    QString theme = themeCombo_->currentData().toString();
    s.setValue("mode", theme);
    s.endGroup();

    s.beginGroup("cloud");
    s.setValue("credentials_path", credentialFilePath_->text());
    s.setValue("max_cost_per_hour", maxCostPerHour_->value());
    s.setValue("max_total_budget", maxTotalBudget_->value());
    s.setValue("autostop_minutes", autostopMinutes_->value());
    s.endGroup();

    Q_EMIT themeChanged(themeCombo_->currentData().toString());
    Q_EMIT noticesChanged();
}

void SettingsDialog::refreshCredentialStatus() {
    QString path = credentialFilePath_->text();
    if (path.isEmpty()) {
        path = QDir::homePath() + "/.maddening/cloud_credentials.yaml";
    }

    QFile file(path);
    if (!file.exists()) {
        credentialStatus_->setText(
            tr("<span style='color:orange'>File not found</span> — "
               "Copy cloud_credentials.example.yaml to %1").arg(path));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        credentialStatus_->setText(
            tr("<span style='color:red'>Cannot read file</span>"));
        return;
    }

    QString content = file.readAll();
    file.close();

    // Simple check: look for provider blocks with non-placeholder keys
    QStringList providers;
    if (content.contains("runpod:") && !content.contains("rp_xxxx")) {
        providers << "RunPod";
    }
    if (content.contains("lambda_labs:") && !content.contains("llxxxx")) {
        providers << "Lambda Labs";
    }

    if (providers.isEmpty()) {
        credentialStatus_->setText(
            tr("<span style='color:orange'>File exists but no configured providers found</span>"));
    } else {
        credentialStatus_->setText(
            tr("<span style='color:green'>Configured:</span> %1").arg(providers.join(", ")));
    }
}

} // namespace microbotica::app
