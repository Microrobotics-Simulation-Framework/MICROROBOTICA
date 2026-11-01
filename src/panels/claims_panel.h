#pragma once

#include <QDockWidget>
#include <QTreeWidget>

#include <map>
#include <string>
#include <vector>

#include "runs/run_folder.h"

namespace microbotica::panels {

/// MIME's claims about a run, shown as MIME states them: the build report's validity lines (each declared and not yet
/// checked, computed at the build, or a capability record's limit), what the run was watched against (a Reynolds
/// bound, a surrogate's range: within or beyond, and why the bound is what it is), the run's flags (each with what it
/// means and whether it was raised), and the experiment's profile with what it still has pending. During a live
/// session, the report comes with hello, and a flag raised in a frame is marked at the time it was first raised.
///
/// A run folder written before MIME 0.3's phase 7n-b has no report (report.json): its watches and flags are shown
/// alone, as they were.
class ClaimsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ClaimsPanel(QWidget* parent = nullptr);

    void clear();

    /// A run folder's claims: its report's validity lines (report.json), and from outputs.json its watches, flags,
    /// the profile's stamp and a state not finite.
    void showRun(const runs::RunFolder& run);

    /// A live session begins: the build report hello gave (or why there is none), and its flags, by path.
    void beginLive(const std::string& experiment, const std::vector<std::string>& flagPaths,
                   const runs::ReportRead& report = {});

    /// A frame's flag values (UI thread): a flag above one half is raised; the first time is kept.
    void liveFlags(double time, const std::map<std::string, double>& values);

    /// How many items say something is beyond its bound or raised (for tests).
    int alarms() const { return alarms_; }

private:
    QTreeWidgetItem* section(const QString& title);
    /// The report's validity lines, or a line saying why there are none.
    void addValidity(const runs::ReportRead& read, bool live);

    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* liveFlags_ = nullptr;
    std::map<std::string, QTreeWidgetItem*> liveItems_;
    std::map<std::string, double> firstRaised_;
    int alarms_ = 0;
};

} // namespace microbotica::panels
