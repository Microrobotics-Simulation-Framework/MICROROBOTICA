#include "panels/claims_panel.h"

#include <QHeaderView>

namespace microbotica::panels {

namespace {
const QColor kAlarm(208, 48, 48);
const QColor kQuiet(58, 154, 58);
const QColor kUnknown(144, 144, 144);

QString q(const std::string& s) { return QString::fromStdString(s); }

QString kindText(const std::string& kind) {
    if (kind == "declared") return QObject::tr("declared, not yet checked");
    if (kind == "computed") return QObject::tr("computed at the build");
    if (kind == "record") return QObject::tr("a capability record's limit");
    return QString::fromStdString(kind);
}
} // namespace

ClaimsPanel::ClaimsPanel(QWidget* parent) : QDockWidget(tr("Claims"), parent) {
    setObjectName("ClaimsPanel");
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({tr("Claim"), tr("State")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->setWordWrap(true);
    tree_->setToolTip(tr("What MIME says of this run: its build report's validity lines, what it was watched against, "
                         "its flags, and what its profile still has pending."));
    setWidget(tree_);
    clear();
}

QTreeWidgetItem* ClaimsPanel::section(const QString& title) {
    auto* item = new QTreeWidgetItem(tree_, {title});
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    item->setExpanded(true);
    return item;
}

void ClaimsPanel::addValidity(const runs::ReportRead& read, bool live) {
    if (!read.report) {
        auto* note = section(tr("Validity lines"));
        if (read.unreadable) {
            new QTreeWidgetItem(note, {tr("The build report is of format %1, which this MICROROBOTICA does not read "
                                          "(it reads %2).").arg(q(*read.unreadable), q(runs::kReportFormat))});
        } else if (live) {
            new QTreeWidgetItem(note, {tr("This MIME's session gives no build report: its run folder states what the "
                                          "run was watched against when the session stops.")});
        } else {
            new QTreeWidgetItem(note, {tr("This run folder has no build report (report.json): a MIME before 0.3's "
                                          "phase 7n-b wrote it. Its watches and flags follow.")});
        }
        return;
    }
    const auto& validity = read.report->validity;
    auto* lines = section(tr("Validity lines (%1)").arg(validity.size()));
    for (const auto& v : validity) {
        auto* item = new QTreeWidgetItem(lines, {q(v.line), kindText(v.kind)});
        item->setToolTip(0, q((v.owner.empty() ? std::string() : v.owner + ": ") + v.line));
        item->setToolTip(1, v.declaredOnly() ? tr("Declared by its entry and not yet checked: a claim, not a verdict.")
                                             : kindText(v.kind));
        if (v.declaredOnly()) item->setForeground(1, kUnknown);
    }
    if (validity.empty()) new QTreeWidgetItem(lines, {tr("The build report states no validity line.")});
}

void ClaimsPanel::clear() {
    tree_->clear();
    liveFlags_ = nullptr;
    liveItems_.clear();
    firstRaised_.clear();
    alarms_ = 0;
    new QTreeWidgetItem(tree_, {tr("No run: run a session, or open a run folder.")});
}

void ClaimsPanel::showRun(const runs::RunFolder& run) {
    tree_->clear();
    alarms_ = 0;
    liveFlags_ = nullptr;
    liveItems_.clear();

    auto* header = section(tr("Run of %1").arg(q(run.experiment())));
    new QTreeWidgetItem(header, {tr("%1 s, %2 steps of %3 s").arg(run.duration()).arg(run.steps()).arg(run.dt())});
    if (const auto nonFinite = run.nonFinite()) {
        auto* item = new QTreeWidgetItem(header, {tr("The state at the end is not finite: %1").arg(q(*nonFinite)),
                                                  tr("not finite")});
        item->setForeground(1, kAlarm);
        ++alarms_;
    }

    const runs::ReportRead report = run.report();
    addValidity(report, false);

    const auto watches = run.watches();
    auto* validity = section(tr("Watched in this run (%1)").arg(watches.size()));
    for (const auto& w : watches) {
        const std::string status = w.raw.value("status", std::string());
        const bool raised = w.raw.value("raised", false);
        auto* item = new QTreeWidgetItem(validity, {q(w.title), q(status.empty() ? (raised ? "raised" : "within") : status)});
        item->setToolTip(0, q(w.title + (w.basis.empty() ? std::string() : "\n\nBasis: " + w.basis)));
        item->setForeground(1, raised ? kAlarm : kQuiet);
        if (raised) ++alarms_;
    }
    if (watches.empty()) new QTreeWidgetItem(validity, {tr("No validity line was watched in this run.")});

    const auto flags = run.flags();
    auto* flagSection = section(tr("Flags (%1)").arg(flags.size()));
    for (const auto& f : flags) {
        QString state;
        QColor colour = kUnknown;
        if (f.wasRaised.has_value()) {
            state = *f.wasRaised ? tr("raised") : tr("not raised");
            colour = *f.wasRaised ? kAlarm : kQuiet;
        } else {
            state = f.raisedAtLastStep ? tr("raised at the last step") : tr("unknown before the last step");
            colour = f.raisedAtLastStep ? kAlarm : kUnknown;
        }
        if (colour == kAlarm) ++alarms_;
        auto* item = new QTreeWidgetItem(flagSection, {q(f.path + (f.entry.empty() ? "" : "  (" + f.entry + ")")), state});
        item->setForeground(1, colour);
        item->setToolTip(0, q(f.text.empty() ? f.meaning : f.text));
        if (!f.meaning.empty()) new QTreeWidgetItem(item, {q(f.meaning)});
    }

    const std::string profile = run.profile();
    const auto pending = run.pending();
    auto* stamp = section(tr("Profile: %1").arg(q(profile.empty() ? std::string("(none)") : profile)));
    if (report.report && report.report->profile && !report.report->profile->stamp.empty()) {
        stamp->setToolTip(0, q(report.report->profile->stamp));
    }
    if (pending.empty()) {
        new QTreeWidgetItem(stamp, {tr("Nothing pending.")});
    } else {
        auto* list = new QTreeWidgetItem(stamp, {tr("Pending (%1)").arg(pending.size())});
        for (const auto& p : pending) new QTreeWidgetItem(list, {q(p)});
    }
}

void ClaimsPanel::beginLive(const std::string& experiment, const std::vector<std::string>& flagPaths,
                            const runs::ReportRead& report) {
    tree_->clear();
    alarms_ = 0;
    liveItems_.clear();
    firstRaised_.clear();
    section(tr("Live session of %1").arg(q(experiment)));
    addValidity(report, true);

    if (report.report) {
        const auto& watches = report.report->watches;
        auto* watched = section(tr("Watched after the run (%1)").arg(watches.size()));
        for (const auto& w : watches) {
            auto* item = new QTreeWidgetItem(watched, {q(w), tr("when it stops")});
            item->setToolTip(0, q(w));
            item->setToolTip(1, tr("Compared with the run when the session stops: its run folder says the outcome."));
            item->setForeground(1, kUnknown);
        }
        if (watches.empty()) new QTreeWidgetItem(watched, {tr("Nothing is watched after the run.")});
    }

    liveFlags_ = section(tr("Flags (%1)").arg(flagPaths.size()));
    for (const auto& path : flagPaths) {
        auto* item = new QTreeWidgetItem(liveFlags_, {q(path), tr("not raised")});
        item->setForeground(1, kQuiet);
        if (report.report) {
            for (const auto& f : report.report->flags) {
                if (f.path != path) continue;
                if (!f.entry.empty()) item->setText(0, q(path + "  (" + f.entry + ")"));
                if (!f.meaning.empty()) {
                    item->setToolTip(0, q(f.meaning));
                    new QTreeWidgetItem(item, {q(f.meaning)});
                }
            }
        }
        liveItems_[path] = item;
    }

    if (report.report && report.report->profile) {
        const auto& p = *report.report->profile;
        auto* stamp = section(tr("Profile: %1").arg(q(p.profile.empty() ? std::string("(none)") : p.profile)));
        if (!p.stamp.empty()) stamp->setToolTip(0, q(p.stamp));
        if (p.pending.empty()) {
            new QTreeWidgetItem(stamp, {tr("Nothing pending.")});
        } else {
            auto* list = new QTreeWidgetItem(stamp, {tr("Pending (%1)").arg(p.pending.size())});
            for (const auto& item : p.pending) new QTreeWidgetItem(list, {q(item)});
        }
    }
}

void ClaimsPanel::liveFlags(double time, const std::map<std::string, double>& values) {
    for (const auto& [path, value] : values) {
        const auto it = liveItems_.find(path);
        if (it == liveItems_.end() || value <= 0.5 || firstRaised_.count(path)) continue;
        firstRaised_[path] = time;
        it->second->setText(1, tr("raised at %1 s").arg(time, 0, 'f', 3));
        it->second->setForeground(1, kAlarm);
        ++alarms_;
    }
}

} // namespace microbotica::panels
