#include "panels/plots_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#ifdef MICROBOTICA_HAS_QTCHARTS
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLegendMarker>
#endif

namespace microbotica::panels {

namespace {
constexpr std::size_t kMaxSamples = 20000;  // a live series keeps its latest samples only
const QColor kFlagColour(220, 60, 60, 70);
} // namespace

PlotsPanel::PlotsPanel(QWidget* parent) : QDockWidget(tr("Plots"), parent) {
    setObjectName("PlotsPanel");
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(2, 2, 2, 2);
    list_ = new QListWidget(left);
    list_->setToolTip(tr("Tick a series to plot it. Flags are shaded where raised."));
    leftLayout->addWidget(list_);
    note_ = new QLabel(tr("No series: run a session, or open a run folder."), left);
    note_->setWordWrap(true);
    leftLayout->addWidget(note_);
    splitter->addWidget(left);

#ifdef MICROBOTICA_HAS_QTCHARTS
    chart_ = new QChart();
    chart_->legend()->setAlignment(Qt::AlignBottom);
    chart_->setMargins(QMargins(4, 4, 4, 4));
    view_ = new QChartView(chart_, splitter);
    view_->setRenderHint(QPainter::Antialiasing);
    splitter->addWidget(view_);
    splitter->setStretchFactor(1, 3);
#else
    auto* fallback = new QLabel(tr("This build has no Qt Charts: the list shows each series' latest value."), splitter);
    fallback->setWordWrap(true);
    fallback->setAlignment(Qt::AlignCenter);
    splitter->addWidget(fallback);
#endif
    setWidget(splitter);

    connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { redraw(); });
    drainTimer_ = new QTimer(this);
    drainTimer_->setInterval(200);  // five redraws a second at most
    connect(drainTimer_, &QTimer::timeout, this, &PlotsPanel::drain);
}

bool PlotsPanel::isFlagName(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    const std::string field = dot == std::string::npos ? path : path.substr(dot + 1);
    return field.find("fired") != std::string::npos || field.rfind("beyond", 0) == 0;
}

void PlotsPanel::clear() {
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pending_.clear();
    }
    drainTimer_->stop();
    series_.clear();
    duration_ = 0.0;
    cursor_ = -1.0;
    rebuildList();
    redraw();
}

void PlotsPanel::beginLive(const std::vector<FieldInfo>& fields) {
    clear();
    for (const auto& f : fields) {
        Series s;
        s.unit = f.unit;
        s.flag = f.flag || isFlagName(f.path);
        series_[f.path] = std::move(s);
    }
    note_->setText(tr("Live: %1 series").arg(series_.size()));
    rebuildList();
    drainTimer_->start();
}

void PlotsPanel::pushSample(double time, const std::unordered_map<std::string, double>& scalars) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pending_.emplace_back(time, scalars);
}

std::size_t PlotsPanel::sampleCount(const std::string& path) const {
    const auto it = series_.find(path);
    return it == series_.end() ? 0 : it->second.values.size();
}

void PlotsPanel::drain() {
    std::vector<std::pair<double, std::unordered_map<std::string, double>>> batch;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        batch.swap(pending_);
    }
    if (batch.empty()) return;
    for (const auto& [time, scalars] : batch) {
        for (const auto& [path, value] : scalars) {
            auto it = series_.find(path);
            if (it == series_.end()) {
                Series s;
                s.flag = isFlagName(path);
                it = series_.emplace(path, std::move(s)).first;
            }
            it->second.times.push_back(time);
            it->second.values.push_back(value);
            if (it->second.values.size() > kMaxSamples) {
                const std::size_t drop = it->second.values.size() - kMaxSamples;
                it->second.times.erase(it->second.times.begin(), it->second.times.begin() + drop);
                it->second.values.erase(it->second.values.begin(), it->second.values.begin() + drop);
            }
        }
        duration_ = std::max(duration_, time);
    }
    if (static_cast<std::size_t>(list_->count()) != series_.size()) {
        rebuildList();
    }
    redraw();
}

void PlotsPanel::showRun(const std::vector<runs::RecordedSeries>& series, double duration) {
    clear();
    for (const auto& r : series) {
        Series s;
        s.unit = r.unit;
        s.description = r.description;
        s.flag = r.flag || isFlagName(r.path);
        s.times = r.times;
        s.values = r.values;
        series_[r.path] = std::move(s);
    }
    duration_ = duration;
    note_->setText(series.empty()
        ? tr("This run recorded no series per step (only its final state). Record them with "
             "run_to(..., record=[...], every=...), or `mime run --record`.")
        : tr("The run's recorded series: %1").arg(series.size()));
    rebuildList();
    redraw();
}

void PlotsPanel::setCursor(double seconds) {
    cursor_ = seconds;
#ifdef MICROBOTICA_HAS_QTCHARTS
    redraw();
#endif
}

void PlotsPanel::rebuildList() {
    // Keep what was ticked; tick the first quantity by default.
    std::map<std::string, bool> ticked;
    for (int i = 0; i < list_->count(); ++i) {
        ticked[list_->item(i)->data(Qt::UserRole).toString().toStdString()] =
            list_->item(i)->checkState() == Qt::Checked;
    }
    const bool firstTime = ticked.empty();
    bool tickedOne = false;
    list_->blockSignals(true);
    list_->clear();
    for (const auto& [path, s] : series_) {
        QString label = QString::fromStdString(path);
        if (!s.unit.empty() && s.unit != "1") label += QStringLiteral(" [%1]").arg(QString::fromStdString(s.unit));
        if (s.flag) label += tr(" (flag)");
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, QString::fromStdString(path));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool on = false;
        if (const auto it = ticked.find(path); it != ticked.end()) {
            on = it->second;
        } else if (firstTime && (s.flag || !tickedOne)) {
            on = true;  // the first quantity, and every flag
            tickedOne = tickedOne || !s.flag;
        }
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        if (!s.description.empty()) item->setToolTip(QString::fromStdString(s.description));
    }
    list_->blockSignals(false);
}

void PlotsPanel::redraw() {
    // Without charts: each series' latest value in the list.
    for (int i = 0; i < list_->count(); ++i) {
        auto* item = list_->item(i);
        const auto it = series_.find(item->data(Qt::UserRole).toString().toStdString());
        if (it == series_.end() || it->second.values.empty()) continue;
        item->setStatusTip(QString::number(it->second.values.back()));
    }
#ifdef MICROBOTICA_HAS_QTCHARTS
    chart_->removeAllSeries();
    for (auto* axis : chart_->axes()) {
        chart_->removeAxis(axis);
        delete axis;
    }
    auto* time = new QValueAxis();
    time->setTitleText(tr("time (s)"));
    chart_->addAxis(time, Qt::AlignBottom);
    double tMin = 0.0;
    double tMax = std::max(duration_, 1e-6);

    std::map<std::string, QValueAxis*> unitAxes;
    QValueAxis* flagAxis = nullptr;
    int unitCount = 0;
    for (int i = 0; i < list_->count(); ++i) {
        auto* item = list_->item(i);
        if (item->checkState() != Qt::Checked) continue;
        const std::string path = item->data(Qt::UserRole).toString().toStdString();
        const auto it = series_.find(path);
        if (it == series_.end() || it->second.values.empty()) continue;
        const Series& s = it->second;
        if (!s.times.empty()) tMin = std::min(tMin, s.times.front());

        if (s.flag) {
            // Shaded where raised: an area from 0 to 1 on a hidden axis of its own.
            if (flagAxis == nullptr) {
                flagAxis = new QValueAxis();
                flagAxis->setRange(0.0, 1.0);
                flagAxis->setVisible(false);
                chart_->addAxis(flagAxis, Qt::AlignRight);
            }
            auto* upper = new QLineSeries();
            auto* lower = new QLineSeries();
            for (std::size_t k = 0; k < s.values.size(); ++k) {
                const double t = s.times[k];
                const double raised = s.values[k] > 0.5 ? 1.0 : 0.0;
                const double next = k + 1 < s.times.size() ? s.times[k + 1] : t;
                upper->append(t, raised);
                upper->append(next, raised);
                lower->append(t, 0.0);
                lower->append(next, 0.0);
            }
            auto* area = new QAreaSeries(upper, lower);
            area->setName(QString::fromStdString(path));
            area->setBrush(kFlagColour);
            area->setPen(QPen(Qt::NoPen));
            chart_->addSeries(area);
            area->attachAxis(time);
            area->attachAxis(flagAxis);
            continue;
        }

        auto* line = new QLineSeries();
        line->setName(QString::fromStdString(path));
        double lo = s.values.front(), hi = s.values.front();
        QList<QPointF> points;
        points.reserve(static_cast<int>(s.values.size()));
        for (std::size_t k = 0; k < s.values.size(); ++k) {
            points.append(QPointF(s.times[k], s.values[k]));
            lo = std::min(lo, s.values[k]);
            hi = std::max(hi, s.values[k]);
        }
        line->replace(points);
        chart_->addSeries(line);
        line->attachAxis(time);
        QValueAxis*& axis = unitAxes[s.unit];
        if (axis == nullptr) {
            axis = new QValueAxis();
            axis->setTitleText(QString::fromStdString(s.unit.empty() ? std::string("value") : s.unit));
            axis->setRange(lo, hi);
            chart_->addAxis(axis, unitCount++ % 2 == 0 ? Qt::AlignLeft : Qt::AlignRight);
        } else {
            axis->setRange(std::min(axis->min(), lo), std::max(axis->max(), hi));
        }
        if (axis->min() == axis->max()) axis->setRange(axis->min() - 1.0, axis->max() + 1.0);
        line->attachAxis(axis);
    }
    time->setRange(tMin, tMax);

    // The playback cursor: a vertical line across the first quantity's axis.
    if (cursor_ >= 0.0 && !unitAxes.empty()) {
        QValueAxis* axis = unitAxes.begin()->second;
        auto* cursor = new QLineSeries();
        cursor->setName(tr("now"));
        cursor->setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
        cursor->append(cursor_, axis->min());
        cursor->append(cursor_, axis->max());
        chart_->addSeries(cursor);
        cursor->attachAxis(time);
        cursor->attachAxis(axis);
        for (auto* marker : chart_->legend()->markers(cursor)) marker->setVisible(false);
    }
#endif
}

} // namespace microbotica::panels
