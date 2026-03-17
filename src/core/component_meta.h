#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace microbotica::core {

/// Stability level for API surfaces and components.
enum class StabilityLevel {
    Experimental,
    Provisional,
    Stable,
    Deprecated
};

NLOHMANN_JSON_SERIALIZE_ENUM(StabilityLevel, {
    {StabilityLevel::Experimental, "experimental"},
    {StabilityLevel::Provisional, "provisional"},
    {StabilityLevel::Stable, "stable"},
    {StabilityLevel::Deprecated, "deprecated"},
})

/// A quantitative bound on a validated parameter range.
struct ValidatedRegime {
    std::string parameter;
    double min_value;
    double max_value;
    std::string units;
    std::string evidence;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ValidatedRegime,
        parameter, min_value, max_value, units, evidence)
};

/// A citable academic or technical reference.
struct Reference {
    std::string key;         ///< BibTeX key in bibliography.bib
    std::string description;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Reference, key, description)
};

/// Structured metadata for a MICROBOTICA component.
///
/// Machine-readable and automatically harvestable for V&V reports,
/// SOUP package documents, and IEC 62304 SOUP assessment.
///
/// The ComponentMeta struct is the C++ equivalent of MADDENING's
/// NodeMeta Python dataclass. It lives in src/core/ to ensure
/// zero Qt/USD/Python dependency. Serializes to JSON via
/// nlohmann_json for CI script consumption.
struct ComponentMeta {
    std::string component_id;            ///< e.g., "MBCA-COMP-001"
    std::string component_version;       ///< e.g., "1.0.0"
    StabilityLevel stability = StabilityLevel::Experimental;
    std::string description;

    // Interface contract
    std::vector<std::string> preconditions;
    std::vector<std::string> postconditions;
    std::vector<std::string> invariants;

    // Design rationale
    std::string design_rationale;

    // Assumptions and constraints
    std::vector<std::string> assumptions;
    std::vector<std::string> limitations;

    // Validated regimes (quantitative, parameter-bound)
    std::vector<ValidatedRegime> validated_regimes;

    // Hazard hints (qualitative, non-parameter-bound)
    // See DOCUMENTATION_ARCHITECTURE.md Section 8.7 for scope distinction.
    std::vector<std::string> hazard_hints;

    // References
    std::vector<Reference> references;

    // Deprecation
    std::optional<std::string> deprecation_notice;
};

inline void to_json(nlohmann::json& j, const ComponentMeta& m) {
    j = nlohmann::json{
        {"component_id", m.component_id},
        {"component_version", m.component_version},
        {"stability", m.stability},
        {"description", m.description},
        {"preconditions", m.preconditions},
        {"postconditions", m.postconditions},
        {"invariants", m.invariants},
        {"design_rationale", m.design_rationale},
        {"assumptions", m.assumptions},
        {"limitations", m.limitations},
        {"validated_regimes", m.validated_regimes},
        {"hazard_hints", m.hazard_hints},
        {"references", m.references},
    };
    if (m.deprecation_notice.has_value())
        j["deprecation_notice"] = m.deprecation_notice.value();
    else
        j["deprecation_notice"] = nullptr;
}

inline void from_json(const nlohmann::json& j, ComponentMeta& m) {
    j.at("component_id").get_to(m.component_id);
    j.at("component_version").get_to(m.component_version);
    j.at("stability").get_to(m.stability);
    j.at("description").get_to(m.description);
    j.at("preconditions").get_to(m.preconditions);
    j.at("postconditions").get_to(m.postconditions);
    j.at("invariants").get_to(m.invariants);
    j.at("design_rationale").get_to(m.design_rationale);
    j.at("assumptions").get_to(m.assumptions);
    j.at("limitations").get_to(m.limitations);
    j.at("validated_regimes").get_to(m.validated_regimes);
    j.at("hazard_hints").get_to(m.hazard_hints);
    j.at("references").get_to(m.references);
    if (j.contains("deprecation_notice") && !j["deprecation_notice"].is_null())
        m.deprecation_notice = j["deprecation_notice"].get<std::string>();
    else
        m.deprecation_notice = std::nullopt;
}

} // namespace microbotica::core
