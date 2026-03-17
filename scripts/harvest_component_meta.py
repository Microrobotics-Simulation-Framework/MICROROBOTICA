#!/usr/bin/env python3
"""Harvest ComponentMeta instances from src/ headers.

Parses C++ source files for ComponentMeta struct initialisations and validates:
- Every ComponentMeta has non-empty component_id, description, stability
- No duplicate component_id values across the codebase
- Outputs JSON to stdout for CI consumption
"""

import json
import re
import sys
from pathlib import Path

SRC_DIR = Path(__file__).parent.parent / "src"

# Regex to find component_id assignments in ComponentMeta initialisations
COMPONENT_ID_RE = re.compile(r'\.component_id\s*=\s*"(MBCA-(?:COMP|IMPL)-\d+)"')
DESCRIPTION_RE = re.compile(r'\.description\s*=\s*"([^"]+)"')
STABILITY_RE = re.compile(r'\.stability\s*=\s*(?:(?:microbotica::)?core::)?StabilityLevel::(\w+)')


def harvest_file(filepath: Path) -> list:
    """Extract ComponentMeta instances from a single file."""
    content = filepath.read_text()
    results = []

    ids = COMPONENT_ID_RE.findall(content)
    descriptions = DESCRIPTION_RE.findall(content)
    stabilities = STABILITY_RE.findall(content)

    for i, cid in enumerate(ids):
        desc = descriptions[i] if i < len(descriptions) else ""
        stab = stabilities[i] if i < len(stabilities) else ""
        results.append({
            "component_id": cid,
            "description": desc,
            "stability": stab,
            "file": str(filepath.relative_to(SRC_DIR.parent)),
        })

    return results


def main():
    if not SRC_DIR.exists():
        print("ERROR: src/ directory not found", file=sys.stderr)
        return 1

    all_components = []
    for header in SRC_DIR.rglob("*.h"):
        all_components.extend(harvest_file(header))
    for source in SRC_DIR.rglob("*.cpp"):
        all_components.extend(harvest_file(source))

    # Deduplicate by component_id (same ID might appear in .h and .cpp)
    seen = {}
    unique_components = []
    for comp in all_components:
        cid = comp["component_id"]
        if cid not in seen:
            seen[cid] = comp
            unique_components.append(comp)

    errors = []
    seen_ids = set()
    for comp in unique_components:
        cid = comp["component_id"]
        if cid in seen_ids:
            errors.append(f"Duplicate component_id: {cid}")
        seen_ids.add(cid)

        if not comp["description"]:
            errors.append(f"{cid}: Empty description")
        if not comp["stability"]:
            errors.append(f"{cid}: Missing stability")

    if errors:
        print(f"FAIL: {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1

    print(json.dumps(unique_components, indent=2))
    print(f"OK: {len(unique_components)} component(s) harvested", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
