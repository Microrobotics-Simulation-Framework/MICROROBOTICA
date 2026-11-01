#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "live/live_protocol.h"

class QDoubleSpinBox;
class QFormLayout;

namespace microbotica::panels {

/// The live-inputs panel: what a running experiment lets you change, and what it does not.
///
/// - **Live inputs**: the controller's declared inputs (mime.live/1 `hello.live_inputs`), each with its units, shape
///   and meaning, editable while the session runs; a value is sent with the protocol's `set` and acts from the
///   controller's next tick. They apply only when the controller is called back (`hello.controller.mode` callback);
///   a controller inside the compiled step takes none, which the panel says.
/// - **Settings**: the experiment's settings, read-only, with their units (the saved folder's lock). A physical setting
///   is never changed live: changing one is a rebuild, a new session from the saved folder.
class LiveInputsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit LiveInputsPanel(QWidget* parent = nullptr);

    /// Show a session's controller and its live inputs.
    void showSession(const std::optional<live::ControllerInfo>& controller,
                     const std::vector<live::LiveInputSpec>& inputs);

    /// The session ended: inputs are no longer settable.
    void endSession();

    /// Show the experiment's settings, read-only: the lock's `resolved.entries` (each entry's `settings`).
    void showSettings(const nlohmann::json& entries);

    /// Clear everything (no experiment open).
    void clearAll();

    /// A `set` was answered: mark the inputs it named as set (accepted) or not (refused).
    void markSet(const nlohmann::json& values, bool accepted);

    /// The values the editors hold now, by label (a number, or a list for a shaped input).
    nlohmann::json currentValue(const std::string& label) const;

Q_SIGNALS:
    /// Send these live inputs: {label: value}.
    void setRequested(const nlohmann::json& values);
    /// Start a new session from the saved folder (a physical setting changed).
    void rebuildRequested();

private:
    struct InputRow {
        live::LiveInputSpec spec;
        std::vector<QDoubleSpinBox*> editors;
        QLabel* state = nullptr;
        QPushButton* send = nullptr;
    };

    QLabel* controllerLabel_ = nullptr;
    QLabel* modeNote_ = nullptr;
    QWidget* inputsBox_ = nullptr;
    QFormLayout* inputsForm_ = nullptr;
    QLabel* noInputs_ = nullptr;
    QTreeWidget* settings_ = nullptr;
    QPushButton* rebuild_ = nullptr;
    std::map<std::string, InputRow> rows_;
    bool sessionLive_ = false;
};

} // namespace microbotica::panels
