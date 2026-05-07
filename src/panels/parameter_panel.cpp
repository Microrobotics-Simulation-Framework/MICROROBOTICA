#include "panels/parameter_panel.h"
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

namespace microbotica::panels {

ParameterPanel::ParameterPanel(QWidget* parent)
    : QDockWidget(tr("Parameters"), parent)
{
    setObjectName("ParameterPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);

    scrollArea_ = new QScrollArea(container);
    scrollArea_->setWidgetResizable(true);

    formContainer_ = new QWidget;
    formLayout_ = new QFormLayout(formContainer_);
    scrollArea_->setWidget(formContainer_);
    layout->addWidget(scrollArea_);

    applyButton_ = new QPushButton(tr("Apply"), container);
    applyButton_->setToolTip(tr("Send parameter changes to MIME"));
    connect(applyButton_, &QPushButton::clicked, this, [this]() {
        Q_EMIT parametersChanged(currentValues());
    });
    layout->addWidget(applyButton_);

    // Empty state
    auto* emptyLabel = new QLabel(tr("No parameters loaded.\n\n"
        "Parameters will appear here when a MIME\nrunner sends its params.py namespace."),
        formContainer_);
    emptyLabel->setWordWrap(true);
    emptyLabel->setAlignment(Qt::AlignCenter);
    formLayout_->addRow(emptyLabel);

    setWidget(container);
}

void ParameterPanel::loadParameters(const nlohmann::json& params) {
    clearForm();

    if (!params.is_object()) return;

    for (auto& [key, value] : params.items()) {
        // Skip private/dunder names
        if (key.size() >= 2 && key[0] == '_') continue;
        createWidget(key, value);
    }
}

nlohmann::json ParameterPanel::currentValues() const {
    nlohmann::json result;

    for (const auto& [name, pw] : widgets_) {
        switch (pw.type) {
            case nlohmann::json::value_t::number_float: {
                auto* spin = qobject_cast<QDoubleSpinBox*>(pw.widget);
                if (spin) result[name] = spin->value();
                break;
            }
            case nlohmann::json::value_t::number_integer:
            case nlohmann::json::value_t::number_unsigned: {
                auto* spin = qobject_cast<QSpinBox*>(pw.widget);
                if (spin) result[name] = spin->value();
                break;
            }
            case nlohmann::json::value_t::boolean: {
                auto* cb = qobject_cast<QCheckBox*>(pw.widget);
                if (cb) result[name] = cb->isChecked();
                break;
            }
            case nlohmann::json::value_t::string: {
                auto* edit = qobject_cast<QLineEdit*>(pw.widget);
                if (edit) result[name] = edit->text().toStdString();
                break;
            }
            default:
                break;
        }
    }

    return result;
}

void ParameterPanel::createWidget(const std::string& name, const nlohmann::json& value) {
    QString label = QString::fromStdString(name);
    ParamWidget pw;
    pw.type = value.type();

    auto liveApply = [this]() {
        Q_EMIT parametersChanged(currentValues());
    };

    switch (value.type()) {
        case nlohmann::json::value_t::number_float: {
            auto* spin = new QDoubleSpinBox;
            spin->setRange(-1e15, 1e15);
            spin->setDecimals(6);
            const double v = value.get<double>();
            spin->setValue(v);
            // Sensible non-zero step: 1% of magnitude, floor 1e-6.
            spin->setSingleStep(std::max(std::abs(v) * 0.01, 1e-6));
            spin->setKeyboardTracking(false);  // emit on commit only
            connect(spin,
                    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, liveApply);
            formLayout_->addRow(label, spin);
            pw.widget = spin;
            break;
        }
        case nlohmann::json::value_t::number_integer:
        case nlohmann::json::value_t::number_unsigned: {
            auto* spin = new QSpinBox;
            spin->setRange(-2147483647, 2147483647);
            spin->setValue(value.get<int>());
            spin->setKeyboardTracking(false);
            connect(spin,
                    QOverload<int>::of(&QSpinBox::valueChanged),
                    this, liveApply);
            formLayout_->addRow(label, spin);
            pw.widget = spin;
            break;
        }
        case nlohmann::json::value_t::boolean: {
            auto* cb = new QCheckBox;
            cb->setChecked(value.get<bool>());
            connect(cb, &QCheckBox::toggled, this, liveApply);
            formLayout_->addRow(label, cb);
            pw.widget = cb;
            break;
        }
        case nlohmann::json::value_t::string: {
            auto* edit = new QLineEdit;
            edit->setText(QString::fromStdString(value.get<std::string>()));
            // Apply only when the user finishes editing (Enter or focus
            // change), not on every keystroke.
            connect(edit, &QLineEdit::editingFinished, this, liveApply);
            formLayout_->addRow(label, edit);
            pw.widget = edit;
            break;
        }
        default: {
            // Unsupported type — display as read-only text
            auto* lbl = new QLabel(QString::fromStdString(value.dump()));
            formLayout_->addRow(label, lbl);
            return; // Don't add to widgets_ map
        }
    }

    widgets_[name] = std::move(pw);
}

void ParameterPanel::clearForm() {
    widgets_.clear();
    // Delete all form widgets
    QLayoutItem* item;
    while ((item = formLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

} // namespace microbotica::panels
