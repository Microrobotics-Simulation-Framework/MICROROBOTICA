#pragma once

#include <string>
#include <QObject>

namespace microbotica::scene {

/// Holds the currently selected prim path.
class PrimSelection : public QObject {
    Q_OBJECT

public:
    explicit PrimSelection(QObject* parent = nullptr) : QObject(parent) {}

    std::string selectedPath() const { return selectedPath_; }

    void select(const std::string& path) {
        if (path != selectedPath_) {
            selectedPath_ = path;
            emit primSelected(selectedPath_);
        }
    }

    void clear() {
        selectedPath_.clear();
        emit primSelected(selectedPath_);
    }

signals:
    void primSelected(const std::string& path);

private:
    std::string selectedPath_;
};

} // namespace microbotica::scene
