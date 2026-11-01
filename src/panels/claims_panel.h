#pragma once

#include <QDockWidget>
#include <QTreeWidget>

#include <map>
#include <string>
#include <vector>

#include "runs/run_folder.h"

namespace microbotica::panels {

/// MIME's claims about a run, shown as MIME states them: the validity lines it watched the run against (a Reynolds
/// bound, a surrogate's range: within or beyond, and why the bound is what it is), the run's flags (each with what it
/// means and whether it was raised), and the experiment's profile with what it still has pending. During a live
/// session, a flag raised in a frame is marked at the time it was first raised.
class ClaimsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ClaimsPanel(QWidget* parent = nullptr);

    void clear();

    /// A run folder's claims (outputs.json): watches, flags, the profile's stamp, a state not finite.
    void showRun(const runs::RunFolder& run);

    /// A live session begins: its flags, by path.
    void beginLive(const std::string& experiment, const std::vector<std::string>& flagPaths);

    /// A frame's flag values (UI thread): a flag above one half is raised; the first time is kept.
    void liveFlags(double time, const std::map<std::string, double>& values);

    /// How many items say something is beyond its bound or raised (for tests).
    int alarms() const { return alarms_; }

private:
    QTreeWidgetItem* section(const QString& title);

    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* liveFlags_ = nullptr;
    std::map<std::string, QTreeWidgetItem*> liveItems_;
    std::map<std::string, double> firstRaised_;
    int alarms_ = 0;
};

} // namespace microbotica::panels
