#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "session_provenance.h"

namespace microbotica::core {

/// Lightweight audit logger for regulatory audit trails.
///
/// Writes structured JSON events to a session-specific audit log.
/// Activated when audit_enabled=true in application config.
/// Uses NullSink by default (zero overhead).
class AuditLogger {
public:
    AuditLogger() = default;
    virtual ~AuditLogger() = default;

    /// Enable audit logging to a file.
    virtual void enable(const std::string& log_path) {
        logger_ = spdlog::basic_logger_mt("audit_" + log_path, log_path);
        logger_->set_pattern("%v");
        enabled_ = true;
    }

    /// Log a structured event.
    virtual void logEvent(const std::string& event_type,
                          const nlohmann::json& data) {
        if (!enabled_) return;
        nlohmann::json entry;
        entry["event"] = event_type;
        entry["data"] = data;
        logger_->info(entry.dump());
    }

    /// Log a session start with full provenance.
    virtual void logSessionStart(const SessionProvenance& provenance) {
        if (!enabled_) return;
        logEvent("session_start", provenance);
    }

    /// Log a parameter change with old and new values.
    virtual void logParameterChange(const std::string& param_name,
                                    const nlohmann::json& old_value,
                                    const nlohmann::json& new_value) {
        if (!enabled_) return;
        nlohmann::json data;
        data["parameter"] = param_name;
        data["old_value"] = old_value;
        data["new_value"] = new_value;
        logEvent("parameter_change", data);
    }

    /// Log receipt of a ResultFrame.
    virtual void logResultFrame(int frame_number, double sim_time,
                                bool any_out_of_range) {
        if (!enabled_) return;
        nlohmann::json data;
        data["frame"] = frame_number;
        data["sim_time"] = sim_time;
        data["out_of_range"] = any_out_of_range;
        logEvent("result_frame", data);
    }

    /// Log a scripting console command.
    virtual void logScriptCommand(const std::string& command,
                                  bool success,
                                  const std::string& error_msg = "") {
        if (!enabled_) return;
        nlohmann::json data;
        data["command"] = command;
        data["success"] = success;
        if (!error_msg.empty()) data["error"] = error_msg;
        logEvent("script_command", data);
    }

protected:
    std::shared_ptr<spdlog::logger> logger_;
    bool enabled_ = false;
};

/// No-op audit logger (zero overhead when auditing is disabled).
class NullAuditLogger : public AuditLogger {
public:
    void enable(const std::string&) override {}
    void logEvent(const std::string&, const nlohmann::json&) override {}
    void logSessionStart(const SessionProvenance&) override {}
    void logParameterChange(const std::string&, const nlohmann::json&,
                            const nlohmann::json&) override {}
    void logResultFrame(int, double, bool) override {}
    void logScriptCommand(const std::string&, bool, const std::string& = "") override {}
};

} // namespace microbotica::core
