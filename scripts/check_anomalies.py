#!/usr/bin/env python3
"""Validate MICROBOTICA known anomalies YAML registry.

Checks:
- All MBCA-ANO-* IDs are unique and follow the prefix convention
- Required fields are non-empty
- Fixed anomalies have a non-null resolution_verification
- memory_safety_relevant=true anomalies have explicit safety_relevance
"""

import sys
import yaml
from pathlib import Path

ANOMALY_FILE = Path(__file__).parent.parent / "docs" / "validation" / "known_anomalies.yaml"
PREFIX = "MBCA-ANO-"

REQUIRED_FIELDS = [
    "anomaly_id", "title", "description", "severity",
    "safety_relevance", "safety_relevance_rationale",
    "affected_components", "affected_versions",
    "resolution_status", "detected_by", "memory_safety_relevant",
    "date_reported",
]

VALID_SEVERITIES = {"critical", "major", "minor", "enhancement"}
VALID_SAFETY_RELEVANCE = {"safety_relevant", "not_safety_relevant", "context_dependent"}
VALID_RESOLUTION_STATUS = {"open", "workaround", "fixed", "wont_fix", "deferred"}
VALID_DETECTED_BY = {"asan", "ubsan", "tsan", "valgrind", "manual_review", "ci_test", "user_report"}


def main():
    if not ANOMALY_FILE.exists():
        print(f"ERROR: {ANOMALY_FILE} not found")
        return 1

    with open(ANOMALY_FILE) as f:
        data = yaml.safe_load(f)

    if not data or "anomalies" not in data:
        print("ERROR: Missing 'anomalies' key in YAML")
        return 1

    errors = []
    seen_ids = set()

    for i, anomaly in enumerate(data["anomalies"]):
        aid = anomaly.get("anomaly_id", f"<missing at index {i}>")

        # Check prefix
        if not aid.startswith(PREFIX):
            errors.append(f"{aid}: ID does not start with '{PREFIX}'")

        # Check uniqueness
        if aid in seen_ids:
            errors.append(f"{aid}: Duplicate anomaly ID")
        seen_ids.add(aid)

        # Check required fields
        for field in REQUIRED_FIELDS:
            val = anomaly.get(field)
            if val is None and field not in ("memory_safety_relevant",):
                errors.append(f"{aid}: Missing required field '{field}'")
            elif isinstance(val, str) and not val.strip():
                errors.append(f"{aid}: Empty required field '{field}'")

        # Check enum values
        severity = anomaly.get("severity", "")
        if severity not in VALID_SEVERITIES:
            errors.append(f"{aid}: Invalid severity '{severity}'")

        safety = anomaly.get("safety_relevance", "")
        if safety not in VALID_SAFETY_RELEVANCE:
            errors.append(f"{aid}: Invalid safety_relevance '{safety}'")

        status = anomaly.get("resolution_status", "")
        if status not in VALID_RESOLUTION_STATUS:
            errors.append(f"{aid}: Invalid resolution_status '{status}'")

        detected = anomaly.get("detected_by", "")
        if detected not in VALID_DETECTED_BY:
            errors.append(f"{aid}: Invalid detected_by '{detected}'")

        # Fixed anomalies must have resolution_verification
        if status == "fixed" and not anomaly.get("resolution_verification"):
            errors.append(f"{aid}: Fixed anomaly missing resolution_verification")

        # memory_safety_relevant=true must have explicit safety_relevance
        if anomaly.get("memory_safety_relevant") is True:
            if not anomaly.get("safety_relevance"):
                errors.append(f"{aid}: memory_safety_relevant=true but no safety_relevance")

    if errors:
        print(f"FAIL: {len(errors)} error(s) in {ANOMALY_FILE}:")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"OK: {len(data['anomalies'])} anomalies validated in {ANOMALY_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
