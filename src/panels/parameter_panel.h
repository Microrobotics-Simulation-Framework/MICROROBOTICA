#pragma once

#include <QDockWidget>
#include <QFormLayout>
#include <QScrollArea>
#include <QPushButton>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

namespace microbotica::panels {

/// Parameter panel — JSON-driven form populated from params.py namespace.
///
/// Displays parameters as editable widgets:
/// - float → QDoubleSpinBox
/// - int → QSpinBox
/// - string → QLineEdit
/// - bool → QCheckBox
///
/// Parameters arrive as a JSON object from the MIME runner (which exec's
/// params.py and serializes the namespace). Edits are collected and
/// emitted as parametersChanged(json) for MimePhysicsProcess::sendParameters().
class ParameterPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ParameterPanel(QWidget* parent = nullptr);

    /// Populate the form from a JSON parameter namespace.
    /// Clears existing widgets and creates new ones.
    void loadParameters(const nlohmann::json& params);

    /// Get the current parameter values as JSON.
    nlohmann::json currentValues() const;

Q_SIGNALS:
    /// Emitted when a parameter value is changed by the user.
    void parametersChanged(const nlohmann::json& params);

private:
    void createWidget(const std::string& name, const nlohmann::json& value);
    void clearForm();
    void onValueChanged();

    QScrollArea* scrollArea_ = nullptr;
    QWidget* formContainer_ = nullptr;
    QFormLayout* formLayout_ = nullptr;
    QPushButton* applyButton_ = nullptr;

    struct ParamWidget {
        QWidget* widget = nullptr;
        nlohmann::json::value_t type;
    };
    std::unordered_map<std::string, ParamWidget> widgets_;
};

} // namespace microbotica::panels
