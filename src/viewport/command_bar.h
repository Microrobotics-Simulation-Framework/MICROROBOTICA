#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace microbotica::viewport {

/// A bar under the 3D viewport holding commands for the user to run in a terminal (MIME's `mime visuals fetch ...`
/// for the meshes its asset cache lacks): each in a read-only field that can be selected, with a Copy button that puts
/// it on the clipboard whole, in one action.
///
/// MBCA-COMP-033
///
/// The viewport's notices (NoticeOverlay, MBCA-COMP-031) are painted on an input-transparent window over the native GL
/// window, so nothing in them can be selected or clicked; a command shown only there could not be copied (the
/// end-to-end walk of 2026-11-05). This bar is an ordinary widget beside the viewport, never over it. The commands are
/// shown exactly as given, as MIME sent them (hello's visuals.fetch, or `mime visuals view`'s line), never rebuilt.
class CommandBar : public QWidget {
    Q_OBJECT

public:
    explicit CommandBar(QWidget* parent = nullptr);

    /// Show `commands` (duplicates once, empty ones dropped) under `caption`; the bar hides when none is left.
    void setCommands(const QString& caption, const QStringList& commands);

    /// Hide the bar and drop its commands.
    void clear();

    /// The commands shown, in order.
    QStringList commands() const { return commands_; }

    /// The caption shown above them.
    QString caption() const;

    /// Put command `index` on the clipboard (and on X11's selection), as its Copy button does.
    void copy(int index);

Q_SIGNALS:
    /// A command was put on the clipboard.
    void copied(const QString& command);

private:
    QLabel* caption_ = nullptr;
    QVBoxLayout* rows_ = nullptr;
    QList<QWidget*> rowWidgets_;
    QStringList commands_;
};

} // namespace microbotica::viewport
