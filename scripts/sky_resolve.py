#!/usr/bin/env python3
"""Resolve SkyPilot cluster endpoints for MICROBOTICA ConnectionManager.

Uses the SkyPilot Python API (not CLI table parsing) for stability.
Outputs structured JSON suitable for ConnectionManager to parse.

Usage:
    sky_resolve.py <cluster_name>   → single JSON object
    sky_resolve.py --list           → JSON array of all clusters

Output format (single cluster):
    {"name": "my-cluster", "ip": "91.199.227.82", "ssh_port": 11420, "status": "UP"}

Output format (--list):
    [{"name": "...", "status": "...", "ip": "...", "ssh_port": ...,
      "cloud": "...", "accelerators": "...", "hourly_cost": ...}, ...]
"""

import json
import sys


def resolve_cluster(cluster_name: str) -> dict:
    """Resolve a single cluster by name."""
    import sky  # type: ignore[import-untyped]

    # sky.status() returns all clusters directly as a list of dicts.
    # No intermediate request ID lookup required.
    clusters = sky.status()

    for cluster in clusters:
        if cluster.get("name") == cluster_name:
            handle = cluster.get("handle", {})
            return {
                "name": cluster_name,
                "ip": getattr(handle, "head_ip", None) or cluster.get("head_ip"),
                "ssh_port": getattr(handle, "head_ssh_port", 22),
                "status": cluster.get("status", "UNKNOWN"),
            }

    return {"name": cluster_name, "ip": None, "ssh_port": None, "status": "NOT_FOUND"}


def list_clusters() -> list:
    """List all clusters with relevant fields."""
    import sky  # type: ignore[import-untyped]

    clusters = sky.status()
    result = []
    for cluster in clusters:
        handle = cluster.get("handle", {})
        result.append({
            "name": cluster.get("name"),
            "status": cluster.get("status", "UNKNOWN"),
            "ip": getattr(handle, "head_ip", None) or cluster.get("head_ip"),
            "ssh_port": getattr(handle, "head_ssh_port", 22),
            "cloud": cluster.get("cloud", "unknown"),
            "accelerators": cluster.get("accelerators", ""),
            "hourly_cost": getattr(handle, "get_hourly_price", lambda: None)()
                if hasattr(handle, "get_hourly_price") else None,
        })
    return result


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: sky_resolve.py <cluster_name> | --list", file=sys.stderr)
        sys.exit(1)

    if sys.argv[1] == "--list":
        print(json.dumps(list_clusters()))
    else:
        print(json.dumps(resolve_cluster(sys.argv[1])))


if __name__ == "__main__":
    main()
