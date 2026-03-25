#!/usr/bin/env python3
"""Convert experiment.yaml to experiment.json for MICROBOTICA C++ consumption.

MICROBOTICA's C++ ExperimentLoader reads JSON (via nlohmann_json) rather than
YAML to avoid adding yaml-cpp as a dependency. This script generates the JSON
from the canonical YAML source.

Usage:
    yaml_to_json.py <experiment_dir>
    yaml_to_json.py experiment.yaml              # explicit file path

The output is written to experiment.json alongside the input YAML.
"""

import json
import os
import sys

import yaml


def convert(yaml_path: str) -> str:
    """Convert a YAML file to JSON and write alongside it."""
    with open(yaml_path) as f:
        data = yaml.safe_load(f)

    json_path = os.path.splitext(yaml_path)[0] + ".json"
    with open(json_path, "w") as f:
        json.dump(data, f, indent=2, default=str)

    return json_path


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: yaml_to_json.py <experiment_dir_or_yaml_path>", file=sys.stderr)
        sys.exit(1)

    target = sys.argv[1]

    if os.path.isdir(target):
        yaml_path = os.path.join(target, "experiment.yaml")
    else:
        yaml_path = target

    if not os.path.exists(yaml_path):
        print(f"Error: {yaml_path} not found", file=sys.stderr)
        sys.exit(1)

    json_path = convert(yaml_path)
    print(json_path)


if __name__ == "__main__":
    main()
