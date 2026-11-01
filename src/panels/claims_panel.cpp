#include "panels/claims_panel.h"

#include <QHeaderView>

namespace microbotica::panels {

namespace {
const QColor kAlarm(208, 48, 48);
const QColor kQuiet(58, 154, 58);
const QColor kUnknown(144, 144, 144);

QString q(const std::string& s) { return QString::fromStdString(s); }
} // namespace

ClaimsPanel::ClaimsPanel(QWidget* parent) : QDockWidget(tr("Claims"), parent) {
    setObjectName("ClaimsPanel");
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({tr("Claim"), tr("State")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->setWordWrap(true);
    tree_->setToolTip(tr("What MIME says of this run: the validity lines it was watched against, its flags, and what "
                         "its profile still has pending."));
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

    const auto watches = run.watches();
    auto* validity = section(tr("Validity (%1)").arg(watches.size()));
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
    if (pending.empty()) {
        new QTreeWidgetItem(stamp, {tr("Nothing pending.")});
    } else {
        auto* list = new QTreeWidgetItem(stamp, {tr("Pending (%1)").arg(pending.size())});
        for (const auto& p : pending) new QTreeWidgetItem(list, {q(p)});
    }
}

void ClaimsPanel::beginLive(const std::string& experiment, const std::vector<std::string>& flagPaths) {
    tree_->clear();
    alarms_ = 0;
    liveItems_.clear();
    firstRaised_.clear();
    section(tr("Live session of %1").arg(q(experiment)));
    liveFlags_ = section(tr("Flags (%1)").arg(flagPaths.size()));
    for (const auto& path : flagPaths) {
        auto* item = new QTreeWidgetItem(liveFlags_, {q(path), tr("not raised")});
        item->setForeground(1, kQuiet);
        liveItems_[path] = item;
    }
    auto* note = section(tr("Validity"));
    new QTreeWidgetItem(note, {tr("MIME states its validity lines in the run folder the session writes when it "
                                  "stops: open it to read them.")});
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
