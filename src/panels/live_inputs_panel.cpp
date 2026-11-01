#include "panels/live_inputs_panel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>

namespace microbotica::panels {

namespace {

QString settingText(const nlohmann::json& value) {
    // A quantity is {value, unit}; anything else is shown as MIME wrote it.
    if (value.is_object() && value.contains("value") && value.contains("unit")) {
        const std::string unit = value["unit"].is_string() ? value["unit"].get<std::string>() : std::string();
        const QString number = QString::fromStdString(value["value"].dump());
        return (unit.empty() || unit == "1") ? number : number + " " + QString::fromStdString(unit);
    }
    if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    }
    return QString::fromStdString(value.dump());
}

void addSetting(QTreeWidgetItem* parent, const std::string& key, const nlohmann::json& value) {
    const bool group = value.is_object() && !(value.contains("value") && value.contains("unit"));
    auto* item = new QTreeWidgetItem(parent, {QString::fromStdString(key), group ? QString() : settingText(value)});
    item->setToolTip(1, item->text(1));
    if (group) {
        for (const auto& [k, v] : value.items()) {
            addSetting(item, k, v);
        }
    }
}

} // namespace

LiveInputsPanel::LiveInputsPanel(QWidget* parent)
    : QDockWidget(tr("Live Inputs"), parent)
{
    setObjectName("LiveInputsPanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    controllerLabel_ = new QLabel(tr("No live session."), container);
    controllerLabel_->setWordWrap(true);
    layout->addWidget(controllerLabel_);

    modeNote_ = new QLabel(container);
    modeNote_->setWordWrap(true);
    modeNote_->setStyleSheet(QStringLiteral("color:#d08a00"));
    modeNote_->hide();
    layout->addWidget(modeNote_);

    auto* inputsGroup = new QGroupBox(tr("Live inputs (sent with set, from the controller's next tick)"), container);
    auto* inputsLayout = new QVBoxLayout(inputsGroup);
    inputsBox_ = new QWidget(inputsGroup);
    inputsForm_ = new QFormLayout(inputsBox_);
    inputsLayout->addWidget(inputsBox_);
    noInputs_ = new QLabel(tr("The controller declares no live inputs."), inputsGroup);
    noInputs_->setWordWrap(true);
    noInputs_->hide();
    inputsLayout->addWidget(noInputs_);
    layout->addWidget(inputsGroup);

    auto* settingsGroup = new QGroupBox(tr("Settings (read-only: a change is a rebuild)"), container);
    auto* settingsLayout = new QVBoxLayout(settingsGroup);
    settings_ = new QTreeWidget(settingsGroup);
    settings_->setHeaderLabels({tr("Setting"), tr("Value")});
    settings_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    settings_->setToolTip(tr("The experiment's settings as its saved folder's lock resolved them. A live session "
                             "never changes one: change it in the experiment and rebuild."));
    settingsLayout->addWidget(settings_);
    rebuild_ = new QPushButton(tr("Rebuild from the saved folder"), settingsGroup);
    rebuild_->setToolTip(tr("Stop this session and start a new one from the saved folder (after its settings "
                            "changed there)."));
    rebuild_->setEnabled(false);
    connect(rebuild_, &QPushButton::clicked, this, &LiveInputsPanel::rebuildRequested);
    settingsLayout->addWidget(rebuild_);
    layout->addWidget(settingsGroup, 1);

    setWidget(container);
}

void LiveInputsPanel::showSession(const std::optional<live::ControllerInfo>& controller,
                                  const std::vector<live::LiveInputSpec>& inputs) {
    while (inputsForm_->rowCount() > 0) {
        inputsForm_->removeRow(0);
    }
    rows_.clear();
    sessionLive_ = true;
    rebuild_->setEnabled(true);

    const bool applies = controller && controller->takesLiveInputs();
    if (controller) {
        controllerLabel_->setText(tr("Controller <b>%1</b>, %2 mode, every %3 base steps")
                                      .arg(QString::fromStdString(controller->entry).toHtmlEscaped(),
                                           QString::fromStdString(controller->mode).toHtmlEscaped())
                                      .arg(controller->every));
    } else {
        controllerLabel_->setText(tr("This experiment has no controller: nothing is settable live."));
    }
    if (controller && !applies) {
        modeNote_->setText(tr("This controller runs inside the compiled step, where its live inputs do not apply. "
                              "Save the experiment with its controller called back (mode=\"callback\") to steer "
                              "it live."));
        modeNote_->show();
    } else {
        modeNote_->hide();
    }

    noInputs_->setVisible(inputs.empty());
    for (const auto& spec : inputs) {
        InputRow row;
        row.spec = spec;
        auto* line = new QWidget(inputsBox_);
        auto* h = new QHBoxLayout(line);
        h->setContentsMargins(0, 0, 0, 0);
        // The session hands the step each input at its default until it is set (MIME's client note, entry 16).
        const std::vector<double> defaults = spec.defaults();
        nlohmann::json shown = spec.defaultValue;
        if (shown.is_null()) {
            shown = (spec.shape.empty() && !defaults.empty()) ? nlohmann::json(defaults.front()) : nlohmann::json(defaults);
        }
        const QString defaultText = QString::fromStdString(shown.dump());
        for (std::size_t i = 0; i < defaults.size(); ++i) {
            auto* spin = new QDoubleSpinBox(line);
            spin->setRange(-1e9, 1e9);
            spin->setDecimals(6);
            spin->setSingleStep(1e-3);
            spin->setValue(defaults[i]);
            spin->setEnabled(applies);
            spin->setToolTip(tr("%1\nDefault: %2, the value at which it changes nothing the step does.")
                                 .arg(QString::fromStdString(spec.meaning), defaultText));
            h->addWidget(spin, 1);
            row.editors.push_back(spin);
        }
        if (!spec.units.empty() && spec.units != "1") {
            h->addWidget(new QLabel(QString::fromStdString(spec.units), line));
        }
        row.send = new QPushButton(tr("Set"), line);
        row.send->setEnabled(applies);
        row.send->setToolTip(tr("Send %1 to the session (set)").arg(QString::fromStdString(spec.label)));
        h->addWidget(row.send);
        row.state = new QLabel(tr("default %1").arg(defaultText), line);
        row.state->setStyleSheet(QStringLiteral("color:#888888"));
        h->addWidget(row.state);
        const std::string label = spec.label;
        connect(row.send, &QPushButton::clicked, this, [this, label]() {
            Q_EMIT setRequested(nlohmann::json{{label, currentValue(label)}});
        });
        auto* name = new QLabel(QString::fromStdString(spec.label), inputsBox_);
        name->setToolTip(QString::fromStdString(spec.meaning));
        inputsForm_->addRow(name, line);
        rows_.emplace(label, std::move(row));
    }
}

void LiveInputsPanel::endSession() {
    sessionLive_ = false;
    for (auto& [label, row] : rows_) {
        for (auto* spin : row.editors) {
            spin->setEnabled(false);
        }
        row.send->setEnabled(false);
    }
    controllerLabel_->setText(controllerLabel_->text() + tr(" — session ended"));
}

void LiveInputsPanel::showSettings(const nlohmann::json& entries) {
    settings_->clear();
    if (!entries.is_object()) {
        return;
    }
    for (const auto& [name, entry] : entries.items()) {
        if (!entry.is_object()) {
            continue;
        }
        QString title = QString::fromStdString(name);
        if (entry.contains("name") && entry["name"].is_string()) {
            title += QStringLiteral("  (%1 %2)")
                         .arg(QString::fromStdString(entry["name"].get<std::string>()),
                              QString::fromStdString(entry.value("version", std::string())));
        }
        auto* top = new QTreeWidgetItem(settings_, {title, QString::fromStdString(entry.value("kind", std::string()))});
        if (entry.contains("settings") && entry["settings"].is_object()) {
            for (const auto& [k, v] : entry["settings"].items()) {
                addSetting(top, k, v);
            }
        }
    }
}

void LiveInputsPanel::clearAll() {
    while (inputsForm_->rowCount() > 0) {
        inputsForm_->removeRow(0);
    }
    rows_.clear();
    settings_->clear();
    sessionLive_ = false;
    rebuild_->setEnabled(false);
    controllerLabel_->setText(tr("No live session."));
    modeNote_->hide();
    noInputs_->hide();
}

void LiveInputsPanel::markSet(const nlohmann::json& values, bool accepted) {
    if (!values.is_object()) {
        return;
    }
    for (const auto& [label, value] : values.items()) {
        auto it = rows_.find(label);
        if (it == rows_.end()) {
            continue;
        }
        it->second.state->setText(accepted ? tr("set: %1").arg(QString::fromStdString(value.dump()))
                                           : tr("refused (see MIME Output)"));
        it->second.state->setStyleSheet(accepted ? QStringLiteral("color:#3a9a3a") : QStringLiteral("color:#d03030"));
    }
}

nlohmann::json LiveInputsPanel::currentValue(const std::string& label) const {
    const auto it = rows_.find(label);
    if (it == rows_.end()) {
        return nullptr;
    }
    const InputRow& row = it->second;
    if (row.spec.shape.empty()) {
        return row.editors.empty() ? nlohmann::json(nullptr) : nlohmann::json(row.editors.front()->value());
    }
    nlohmann::json list = nlohmann::json::array();
    for (auto* spin : row.editors) {
        list.push_back(spin->value());
    }
    return list;
}

} // namespace microbotica::panels
