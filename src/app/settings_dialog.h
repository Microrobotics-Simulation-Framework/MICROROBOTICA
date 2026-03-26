#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>

namespace microbotica::app {

/// Application settings dialog with General, Theme, and Cloud tabs.
///
/// Stores preferences in QSettings. Credentials remain in
/// ~/.maddening/cloud_credentials.yaml (MADDENING's convention) —
/// this dialog shows credential status, not credential values.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    /// Load current settings into the dialog widgets.
    void loadSettings();

    /// Save dialog state to QSettings.
    void saveSettings();

Q_SIGNALS:
    /// Emitted when the theme changes and the caller should re-apply the palette.
    void themeChanged(const QString& theme);

private:
    void createGeneralTab(QTabWidget* tabs);
    void createThemeTab(QTabWidget* tabs);
    void createCloudTab(QTabWidget* tabs);
    void refreshCredentialStatus();

    // General tab
    QLineEdit* defaultExperimentDir_ = nullptr;
    QSpinBox* zmqPubPort_ = nullptr;
    QSpinBox* zmqReqPort_ = nullptr;

    // Theme tab
    QComboBox* themeCombo_ = nullptr;

    // Cloud tab
    QLineEdit* credentialFilePath_ = nullptr;
    QLabel* credentialStatus_ = nullptr;
    QDoubleSpinBox* maxCostPerHour_ = nullptr;
    QDoubleSpinBox* maxTotalBudget_ = nullptr;
    QSpinBox* autostopMinutes_ = nullptr;
    QLabel* sshKeyPath_ = nullptr;
};

} // namespace microbotica::app
