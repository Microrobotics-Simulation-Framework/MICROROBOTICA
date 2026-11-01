#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QTimer>

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runs/run_folder.h"

class QLabel;
class QStackedWidget;

#ifdef MICROBOTICA_HAS_QTCHARTS
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#endif

namespace microbotica::panels {

/// The plots panel: a run's observation outputs and flags over time, in their units, live or from a run folder.
///
/// - **Live**: every scalar a session's frames carry (an observation's per-step output, a flag), sample by sample;
///   samples arrive on the frames thread (pushSample) and are drawn a few times a second.
/// - **A run folder**: the series the run recorded per step (outputs.npz); a run that recorded none has none, which
///   the panel says. During playback, a cursor follows the recording's time.
/// - **Flags** (a surrogate's tilt, a joint beyond its speed limit) are shaded where raised, not drawn as lines.
///
/// Drawn with Qt Charts when the build has it; otherwise the panel lists each series with its latest value.
class PlotsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit PlotsPanel(QWidget* parent = nullptr);

    /// Forget every series.
    void clear();

    /// A live session begins: its scalar fields, by path, with their units; `flag` says which are flags.
    struct FieldInfo {
        std::string path;
        std::string unit;
        bool flag = false;
    };
    void beginLive(const std::vector<FieldInfo>& fields);

    /// One frame's scalars (any thread).
    void pushSample(double time, const std::unordered_map<std::string, double>& scalars);

    /// A run folder's recorded series (and the run's duration, for the axis), replacing what is shown.
    void showRun(const std::vector<runs::RecordedSeries>& series, double duration);

    /// The playback position, in the run's seconds.
    void setCursor(double seconds);

    /// The series shown, by path (for tests and the scripting console).
    std::size_t seriesCount() const { return series_.size(); }
    std::size_t sampleCount(const std::string& path) const;

private:
    struct Series {
        std::string unit;
        std::string description;
        bool flag = false;
        std::vector<double> times;
        std::vector<double> values;
    };

    void drain();
    void rebuildList();
    void redraw();
    static bool isFlagName(const std::string& path);

    std::map<std::string, Series> series_;
    double duration_ = 0.0;
    double cursor_ = -1.0;

    std::mutex pendingMutex_;
    std::vector<std::pair<double, std::unordered_map<std::string, double>>> pending_;
    QTimer* drainTimer_ = nullptr;

    QListWidget* list_ = nullptr;
    QLabel* note_ = nullptr;
#ifdef MICROBOTICA_HAS_QTCHARTS
    QChart* chart_ = nullptr;
    QChartView* view_ = nullptr;
#endif
};

} // namespace microbotica::panels
