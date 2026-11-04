#pragma once

// MIME's visuals as a viewer shows them. Some descriptions' meshes cannot be redistributed (the AR4's are Annin
// Robotics' design): MIME ships their records only, and `mime visuals fetch` downloads and bakes them on the user's
// machine, into MIME's asset cache. An overlay in that cache draws them over a stage (`hello.visuals`, `mime visuals
// view`); without it the links draw as simple shapes. Either way each set's notice (its credit and terms) is shown.
//
// What to fetch is named by folder: a saved or run folder's lock records the manifests of the meshes its stage draws
// (MIME's saved form 6), so `mime visuals fetch <folder>` works wherever MIME is installed. A set's name works only
// where an installed package ships that set, which MIME installed from PyPI does not for the AR4.
//
// Free of Qt and USD: the standard library only.

#include <map>
#include <string>
#include <vector>

namespace microbotica::runs {

/// `text` as one word for a POSIX shell: unchanged when every character is one a shell takes literally, otherwise in
/// single quotes (a single quote inside written as '\'').
std::string shellQuoted(const std::string& text);

/// The command that fetches and bakes the meshes a saved or run folder's stage draws: `mime visuals fetch <folder>`,
/// the folder quoted for a shell, to paste as it is.
std::string visualsFetchCommand(const std::string& folder);

/// The saved or run folder a stage or recording is in, found as MIME finds it: the file's grandparent or parent,
/// whichever holds `mime.lock` (a saved folder's) or `experiment/mime.lock` (a run folder's), the grandparent first.
/// Empty when neither does.
std::string folderOfStage(const std::string& stagePath);

/// What `mime visuals view <target>` printed on standard output: what the asset cache draws and lacks, each set's
/// notice, and, last, the layer that opens the target (the target itself when the cache draws nothing of it).
struct VisualsView {
    std::string layer;
    std::string overlay;                   ///< the overlay alone, where the cache draws any mesh: to add over the stage
    long drawn = 0;                        ///< meshes drawn from the asset cache
    std::map<std::string, long> missing;   ///< the sets the cache lacks, and how many meshes of each
    std::vector<std::string> lines;        ///< every line before the layer, as printed (for the console)
    std::vector<std::string> notice;       ///< the lines that are neither a count nor a missing set: the notices
};

/// Read `mime visuals view`'s output. Its counts are found by the lines MIME writes ("<n> meshes drawn from the asset
/// cache (<overlay>)", "<n> meshes of <set> are not in the asset cache: ..."); a line of another shape is kept as
/// notice, so a wording MIME changes reads as nothing drawn and nothing missing, never as a wrong count.
VisualsView parseVisualsView(const std::string& output);

/// What to show beside a set's meshes while they are drawn: each notice's `credit:` and `licence:` lines, with the line
/// before its credit (what the set is), or every line when no notice has a credit or licence line. `notices` as hello's
/// `visuals.notice` gives them (live_protocol's VisualsInfo::noticeLines) or as parseVisualsView keeps them: text that
/// may hold line breaks, read line by line.
std::vector<std::string> noticeCredit(const std::vector<std::string>& notices);

} // namespace microbotica::runs
