#pragma once

// A MIME 0.3 run folder (MIME's results/README.md, docs/live.md): what `run_to`, `mime run` or a live session wrote,
// read for MICROROBOTICA's playback, plots and claims. Its experiment (`experiment/`, a saved folder), its recording
// (`recording.usdc` over `experiment/scene/scene.usda`), its outputs (`outputs.json`, the manifest, and
// `outputs.npz`, the arrays), its record (`run.json`), its topology (`topology.json`) and, for a live session, its
// command log (`commands.jsonl`).
//
// Free of Qt and USD: the standard library, nlohmann_json and the npz reader.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/component_meta.h"

namespace microbotica::runs {

/// A flag of the run (a surrogate's tilt, a joint beyond its speed limit): what it means and what the run says of it.
struct RunFlag {
    std::string path;      ///< e.g. "drag:umr.tilt_fired"
    std::string entry;     ///< the entry that raises it, e.g. "drag 0.3.0"
    std::string meaning;
    std::string text;      ///< MIME's sentence on the run's flag: raised when, or unknown and why
    std::optional<bool> wasRaised;  ///< over the run: true, false, or unknown (not recorded per step)
    bool raisedAtLastStep = false;
};

/// A claim's limit the run is watched against (a validity line: Stokes flow's Reynolds bound, a surrogate's range).
struct RunWatch {
    std::string title;     ///< what is watched, as MIME names it
    std::string basis;     ///< why the bound is what it is
    nlohmann::json raw;    ///< the whole record
};

/// A series recorded over the run: its values at its steps, in its unit.
struct RecordedSeries {
    std::string path;      ///< "<node>.<field>", e.g. "swim_speed:umr.speed"
    std::string unit;
    std::string description;
    bool flag = false;     ///< a flag's 0/1 track rather than a quantity
    std::vector<double> times;   ///< seconds (step x dt)
    std::vector<double> values;  ///< one per time (a scalar series); the first component otherwise
};

/// MBCA-COMP-070
class RunFolder {
public:
    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-COMP-070",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "A MIME 0.3 run folder read for playback, plots and claims.",
            .preconditions     = {"The folder holds outputs.json or run.json, as MIME 0.3 writes them"},
            .postconditions    = {"Recorded series come back at their steps' times, in their units",
                                  "Flags and validity watches come back as MIME states them"},
            .invariants        = {"The folder is only read"},
            .design_rationale  = "One reader of MIME's run-folder layout (results/README.md), shared by the "
                                 "plots and claims panels and the playback of recordings.",
            .assumptions       = {"outputs.npz holds plain numeric arrays (MIME writes it with allow_pickle=False)"},
            .limitations       = {"A run that recorded no series per step has none to plot (its final state only)",
                                  "The build report's validity lines are not in a run folder: its watches are"},
            .validated_regimes = {},
            .hazard_hints      = {"A layout MIME changes reads as fewer series, not as wrong ones: each is "
                                  "found by the name the manifest gives"},
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    /// Whether `folder` looks like a run folder: it holds outputs.json or run.json.
    static bool looksLikeRunFolder(const std::string& folder);

    /// The run folder `path` is in, when `path` is a file in one (its recording.usdc), or itself; empty otherwise.
    static std::string runFolderOf(const std::string& path);

    /// Read the folder's manifest and record. Throws std::runtime_error when it has neither.
    explicit RunFolder(std::string folder);

    const std::string& folder() const { return folder_; }
    std::string experiment() const;           ///< the experiment's name
    std::string experimentFolder() const;     ///< experiment/ (a saved folder), or empty
    std::string recording() const;            ///< recording.usdc, or empty
    std::string stage() const;                ///< experiment/scene/scene.usda, or empty
    std::string topology() const;             ///< topology.json, or empty
    std::string commandsLog() const;          ///< commands.jsonl, or empty (not a live session's)
    std::string outputsArchive() const;       ///< outputs.npz, or empty

    double dt() const;                        ///< the base step (s)
    std::uint64_t steps() const;
    double duration() const;                  ///< simulated seconds
    const nlohmann::json& outputs() const { return outputs_; }   ///< outputs.json
    const nlohmann::json& record() const { return run_; }        ///< run.json

    std::vector<RunFlag> flags() const;
    std::vector<RunWatch> watches() const;
    std::string profile() const;              ///< the experiment's profile (exploratory, ...)
    std::vector<std::string> pending() const; ///< what the profile's stamp says is still pending
    std::optional<std::string> nonFinite() const;  ///< what MIME says when the state ended not finite

    /// Every series the run recorded per step (record=..., every=...), read from outputs.npz. Empty when it recorded
    /// none (a run keeps its final state alone unless asked).
    std::vector<RecordedSeries> recordedSeries() const;

private:
    std::string file(const char* name) const;

    std::string folder_;
    nlohmann::json outputs_;
    nlohmann::json run_;
};

} // namespace microbotica::runs
