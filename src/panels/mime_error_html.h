#pragma once

#include <QString>

#include "core/mime_error.h"

namespace microbotica::panels {

/// Colour of a refusal's class: who can act on it (MIME 0.3, docs/errors.md).
inline QString refusalClassColour(const std::string& cls) {
    if (cls == "user") return QStringLiteral("#d08a00");         // the script can change it
    if (cls == "environment") return QStringLiteral("#2f7fd0");  // something to install, fetch or attach
    if (cls == "internal") return QStringLiteral("#d03030");     // a defect, MIME's or a lab package's
    return QStringLiteral("#909090");                            // unrecognised, from below
}

/// One of MIME's coded errors as the console shows it: what was refused, then each reason with its class and code,
/// what happened, why, what to do, where, and its documentation page as a link. Text is HTML-escaped.
inline QString mimeErrorReportHtml(const core::MimeErrorReport& report) {
    auto esc = [](const std::string& s) { return QString::fromStdString(s).toHtmlEscaped(); };
    QString html;
    if (!report.headline.empty()) {
        html += QStringLiteral("<b style=\"color:#d03030\">%1</b><br>").arg(esc(report.headline));
    }
    const QString indent = QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
    for (const auto& r : report.refusals) {
        const QString colour = refusalClassColour(r.cls);
        html += QStringLiteral("<span style=\"background-color:%1;color:#ffffff\">&nbsp;%2&nbsp;</span> "
                               "<b>%3</b>: %4<br>")
                    .arg(colour, esc(r.cls.empty() ? std::string("error") : r.cls), esc(r.code), esc(r.message));
        if (!r.why.empty()) {
            html += indent + QStringLiteral("<i>why:</i> %1<br>").arg(esc(r.why));
        }
        if (!r.action.empty()) {
            html += indent + QStringLiteral("<b>what to do:</b> <code>%1</code><br>").arg(esc(r.action));
        }
        if (!r.where.empty()) {
            html += indent + QStringLiteral("<i>at:</i> %1<br>").arg(esc(r.where));
        }
        if (!r.page.empty()) {
            const QString page = esc(r.page);
            if (r.page.rfind("http://", 0) == 0 || r.page.rfind("https://", 0) == 0) {
                html += indent + QStringLiteral("<i>see:</i> <a href=\"%1\">%1</a><br>").arg(page);
            } else {
                html += indent + QStringLiteral("<i>see:</i> %1<br>").arg(page);
            }
        }
    }
    if (!report.tracebackNote.empty()) {
        html += QStringLiteral("<span style=\"color:#888888\">%1</span>")
                    .arg(esc(report.tracebackNote).replace(QStringLiteral("\n"), QStringLiteral("<br>")));
    }
    return html;
}

} // namespace microbotica::panels
