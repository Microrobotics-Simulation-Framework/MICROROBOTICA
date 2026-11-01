#include "runs/run_folder.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "runs/npz.h"

namespace fs = std::filesystem;

namespace microbotica::runs {

namespace {

nlohmann::json readJson(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return nlohmann::json();
    }
    try {
        return nlohmann::json::parse(in);
    } catch (const nlohmann::json::parse_error&) {
        return nlohmann::json();
    }
}

std::string text(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) return {};
    const auto it = object.find(key);
    return (it != object.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

} // namespace

bool RunFolder::looksLikeRunFolder(const std::string& folder) {
    std::error_code ec;
    return fs::is_directory(folder, ec)
        && (fs::exists(fs::path(folder) / "outputs.json", ec) || fs::exists(fs::path(folder) / "run.json", ec));
}

std::string RunFolder::runFolderOf(const std::string& path) {
    std::error_code ec;
    const fs::path p(path);
    if (fs::is_directory(p, ec)) {
        return looksLikeRunFolder(p.string()) ? p.string() : std::string();
    }
    const fs::path parent = p.parent_path();
    return looksLikeRunFolder(parent.string()) ? parent.string() : std::string();
}

RunFolder::RunFolder(std::string folder) : folder_(std::move(folder)) {
    outputs_ = readJson(fs::path(folder_) / "outputs.json");
    run_ = readJson(fs::path(folder_) / "run.json");
    if (!outputs_.is_object() && !run_.is_object()) {
        throw std::runtime_error(folder_ + " is not a run folder: no outputs.json or run.json");
    }
}

std::string RunFolder::file(const char* name) const {
    std::error_code ec;
    const fs::path p = fs::path(folder_) / name;
    return fs::exists(p, ec) ? p.string() : std::string();
}

std::string RunFolder::experiment() const {
    const std::string fromOutputs = text(outputs_, "experiment");
    return fromOutputs.empty() ? fs::path(folder_).filename().string() : fromOutputs;
}

std::string RunFolder::experimentFolder() const {
    std::error_code ec;
    const fs::path p = fs::path(folder_) / "experiment";
    return fs::exists(p / "experiment.mime.yaml", ec) ? p.string() : std::string();
}

std::string RunFolder::recording() const {
    // run.json names it (a run_to with stage_every, or a session that recorded).
    const std::string named = text(run_, "recording");
    if (!named.empty()) {
        std::error_code ec;
        const fs::path p = fs::path(folder_) / named;
        if (fs::exists(p, ec)) return p.string();
    }
    return file("recording.usdc");
}

std::string RunFolder::stage() const { return file("experiment/scene/scene.usda"); }
std::string RunFolder::topology() const { return file("topology.json"); }
std::string RunFolder::commandsLog() const { return file("commands.jsonl"); }
std::string RunFolder::outputsArchive() const { return file("outputs.npz"); }

double RunFolder::dt() const {
    if (outputs_.is_object() && outputs_.contains("dt") && outputs_["dt"].is_number()) {
        return outputs_["dt"].get<double>();
    }
    return 0.0;
}

std::uint64_t RunFolder::steps() const {
    for (const auto* j : {&outputs_, &run_}) {
        if (j->is_object() && j->contains("steps") && (*j)["steps"].is_number()) {
            return (*j)["steps"].get<std::uint64_t>();
        }
    }
    return 0;
}

double RunFolder::duration() const {
    if (run_.is_object() && run_.contains("duration") && run_["duration"].is_number()) {
        return run_["duration"].get<double>();
    }
    return static_cast<double>(steps()) * dt();
}

std::vector<RunFlag> RunFolder::flags() const {
    std::vector<RunFlag> out;
    if (!outputs_.is_object()) return out;
    // flags: what each flag means; run_flags: what the run says of each, in a sentence.
    if (outputs_.contains("flags") && outputs_["flags"].is_array()) {
        for (const auto& f : outputs_["flags"]) {
            RunFlag flag;
            flag.path = text(f, "path");
            flag.entry = text(f, "label");
            flag.meaning = text(f, "meaning");
            if (f.contains("was_raised") && f["was_raised"].is_boolean()) flag.wasRaised = f["was_raised"].get<bool>();
            flag.raisedAtLastStep = f.value("raised_at_last_step", false);
            out.push_back(std::move(flag));
        }
    }
    if (outputs_.contains("run_flags") && outputs_["run_flags"].is_array()) {
        for (const auto& f : outputs_["run_flags"]) {
            const std::string path = text(f, "flag");
            RunFlag* match = nullptr;
            for (auto& flag : out) {
                if (flag.path == path) match = &flag;
            }
            if (match == nullptr) {
                out.push_back(RunFlag{});
                match = &out.back();
                match->path = path;
                match->entry = text(f, "entry");
            }
            match->text = text(f, "text");
            if (f.contains("was_raised") && f["was_raised"].is_boolean()) match->wasRaised = f["was_raised"].get<bool>();
            match->raisedAtLastStep = f.value("raised_at_last_step", match->raisedAtLastStep);
        }
    }
    return out;
}

std::vector<RunWatch> RunFolder::watches() const {
    std::vector<RunWatch> out;
    if (!outputs_.is_object() || !outputs_.contains("watches") || !outputs_["watches"].is_array()) return out;
    for (const auto& w : outputs_["watches"]) {
        RunWatch watch;
        watch.title = text(w, "text");
        if (watch.title.empty()) {
            watch.title = text(w, "owner") + " (" + text(w, "entry") + ")";
        }
        watch.basis = text(w, "basis");
        watch.raw = w;
        out.push_back(std::move(watch));
    }
    return out;
}

std::string RunFolder::profile() const {
    if (outputs_.is_object() && outputs_.contains("stamp")) return text(outputs_["stamp"], "profile");
    return {};
}

std::vector<std::string> RunFolder::pending() const {
    std::vector<std::string> out;
    if (outputs_.is_object() && outputs_.contains("stamp") && outputs_["stamp"].contains("pending")
        && outputs_["stamp"]["pending"].is_array()) {
        for (const auto& p : outputs_["stamp"]["pending"]) {
            if (p.is_string()) out.push_back(p.get<std::string>());
        }
    }
    return out;
}

std::optional<std::string> RunFolder::nonFinite() const {
    for (const auto* j : {&run_, &outputs_}) {
        if (j->is_object() && j->contains("non_finite") && !(*j)["non_finite"].is_null()) {
            const auto& v = (*j)["non_finite"];
            return v.is_string() ? v.get<std::string>() : v.dump();
        }
    }
    return std::nullopt;
}

std::vector<RecordedSeries> RunFolder::recordedSeries() const {
    // outputs.json says where each recorded series is in outputs.npz (MIME's results/README.md):
    // - an observation's per-step output recorded at a spacing: `values` and `steps` (the step of each sample);
    // - a flag recorded every step: `array`, one value a base step, the k-th the state at the end of step k + 1.
    // A sample at step k is at time k dt.
    std::vector<RecordedSeries> out;
    const std::string archive = outputsArchive();
    if (!outputs_.is_object() || archive.empty()) return out;
    const NpzFile npz(archive);
    const double step = dt();

    if (outputs_.contains("observations") && outputs_["observations"].is_array()) {
        for (const auto& observation : outputs_["observations"]) {
            if (!observation.contains("outputs") || !observation["outputs"].is_array()) continue;
            for (const auto& o : observation["outputs"]) {
                const std::string values = text(o, "values");
                const std::string steps = text(o, "steps");
                if (!(o.contains("recorded") && o["recorded"].is_boolean() && o["recorded"].get<bool>())
                    || values.empty() || !npz.contains(values)) {
                    continue;
                }
                RecordedSeries series;
                series.path = text(o, "path");
                series.unit = text(o, "unit");
                series.description = text(o, "description");
                const NpyArray v = npz.read(values);
                const NpyArray k = (!steps.empty() && npz.contains(steps)) ? npz.read(steps) : NpyArray{};
                const std::size_t samples = v.shape.empty() ? 1 : v.shape.front();
                const std::size_t width = samples ? v.count() / samples : 0;
                for (std::size_t i = 0; i < samples && width > 0; ++i) {
                    series.values.push_back(v.values[i * width]);
                    series.times.push_back((i < k.values.size() ? k.values[i] : static_cast<double>(i + 1)) * step);
                }
                out.push_back(std::move(series));
            }
        }
    }
    if (outputs_.contains("flags") && outputs_["flags"].is_array()) {
        for (const auto& f : outputs_["flags"]) {
            const std::string array = text(f, "array");
            if (array.empty() || !npz.contains(array)) continue;
            RecordedSeries series;
            series.path = text(f, "path");
            series.unit = "1";
            series.description = text(f, "meaning");
            series.flag = true;
            const NpyArray v = npz.read(array);
            series.values = v.values;
            series.times.resize(v.values.size());
            for (std::size_t i = 0; i < v.values.size(); ++i) {
                series.times[i] = static_cast<double>(i + 1) * step;
            }
            out.push_back(std::move(series));
        }
    }
    return out;
}

} // namespace microbotica::runs
