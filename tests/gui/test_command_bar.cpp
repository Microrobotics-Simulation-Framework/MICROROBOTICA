// MBCA-VER-021: the command bar under the viewport (MBCA-COMP-033) shows the command a notice asks for exactly as given
// (MIME's fetch command: its folder quoted for a shell by MIME), in a field that can be selected, and its Copy button
// puts the whole command on the clipboard in one action. In the end-to-end walk of 2026-11-05 the command was shown only
// on the input-transparent notice overlay, where it could be neither selected nor copied. Runs on any Qt platform.

#include <catch2/catch_test_macros.hpp>

#include "gui/gui_app.h"
#include "viewport/command_bar.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPushButton>

using namespace microbotica;

TEST_CASE("MBCA-VER-021: the command bar shows MIME's command as given, selectable, copied whole in one click", "[gui]") {
    testing::guiApp();
    const QString command = QStringLiteral("mime visuals fetch '/data/My Runs/it'\"'\"'s a0_run/'");

    viewport::CommandBar bar;
    int copied = 0;
    QString copiedText;
    QObject::connect(&bar, &viewport::CommandBar::copied, [&](const QString& text) {
        ++copied;
        copiedText = text;
    });
    CHECK(bar.isHidden());

    // Duplicates once (several sets fetched by one folder's command), blank ones dropped.
    bar.setCommands(QStringLiteral("To draw the meshes, run this in a terminal:"), {command, command, QStringLiteral("  ")});
    CHECK_FALSE(bar.isHidden());
    CHECK(bar.commands() == QStringList{command});
    CHECK(bar.caption() == QStringLiteral("To draw the meshes, run this in a terminal:"));

    const QList<QLineEdit*> fields = bar.findChildren<QLineEdit*>(QStringLiteral("commandBarField"));
    REQUIRE(fields.size() == 1);
    CHECK(fields[0]->text() == command);
    CHECK(fields[0]->isReadOnly());
    fields[0]->selectAll();
    CHECK(fields[0]->selectedText() == command);

    const QList<QPushButton*> buttons = bar.findChildren<QPushButton*>(QStringLiteral("commandBarCopy"));
    REQUIRE(buttons.size() == 1);
    QGuiApplication::clipboard()->setText(QStringLiteral("something else"));
    buttons[0]->click();
    CHECK(QGuiApplication::clipboard()->text() == command);
    CHECK(copied == 1);
    CHECK(copiedText == command);

    // Each command its own row and button.
    const QString other = QStringLiteral("mime visuals fetch /data/other_saved/");
    bar.setCommands(QStringLiteral("Two folders:"), {command, other});
    REQUIRE(bar.findChildren<QPushButton*>(QStringLiteral("commandBarCopy")).size() == 2);
    bar.copy(1);
    CHECK(QGuiApplication::clipboard()->text() == other);

    bar.clear();
    CHECK(bar.isHidden());
    CHECK(bar.commands().isEmpty());
    bar.copy(0);  // nothing to copy: no-op
    CHECK(copied == 2);
}
