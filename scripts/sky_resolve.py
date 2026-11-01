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


def _materialise(req):
    """SkyPilot 0.12.x switched to an async API: most calls return a
    RequestId that the caller must resolve via sky.get / sky.stream_and_get.
    Older versions returned the value directly. This wrapper supports both.
    """
    import sky  # type: ignore[import-untyped]
    if type(req).__name__ == "RequestId":
        if hasattr(sky, "get"):
            return sky.get(req)
        if hasattr(sky, "stream_and_get"):
            return sky.stream_and_get(req)
    return req


def _status_str(value) -> str:
    """SkyPilot used to return plain strings (`'UP'`); 0.12.x returns
    enum-stringified `'ClusterStatus.UP'`. Normalise to the bare token
    so the C++ consumer's `status == "UP"` check stays correct."""
    s = str(value)
    return s.split(".")[-1] if "." in s else s


def _to_mapping(cluster):
    """Cluster items used to be plain dicts; newer SkyPilot returns
    `StatusResponse` Pydantic models. Normalise to a dict-like view."""
    if isinstance(cluster, dict):
        return cluster
    if hasattr(cluster, "model_dump"):
        return cluster.model_dump()
    if hasattr(cluster, "dict"):
        return cluster.dict()
    # Fallback: best-effort attribute lookup wrapped in a tiny shim.
    class _Attr:
        def __init__(self, obj):
            self._o = obj
        def get(self, k, default=None):
            return getattr(self._o, k, default)
    return _Attr(cluster)


def _kubectl_lb_ip(cluster_name: str) -> "str | None":
    """For Kubernetes-backed clusters (SSH node pools, GKE, etc.) the
    `handle.head_ip` is the pod's internal cluster IP, which a host
    outside the cluster cannot reach. The SkyPilot LoadBalancer service
    exposes the task's `ports:` on an externally-routable IP (the node
    IP for SSH pools, a cloud LB for managed k8s). Query it via kubectl.
    """
    import json
    import subprocess  # local import keeps the non-k8s path lightweight
    try:
        # List every LB service and match either `<name>-skypilot-lb` or
        # `<name>--skypilot-lb` (SkyPilot's naming has fluctuated). For
        # SSH node pools the LB's ingress IP is the node IP.
        out = subprocess.check_output(
            ["kubectl", "get", "svc", "-o", "json"],
            stderr=subprocess.DEVNULL, timeout=10,
        )
        items = json.loads(out.decode()).get("items", [])
        for svc in items:
            name = svc.get("metadata", {}).get("name", "")
            if "skypilot-lb" not in name:
                continue
            if not (name.startswith(cluster_name + "-")
                    or name.startswith(cluster_name + "--")):
                continue
            ingress = (svc.get("status", {})
                          .get("loadBalancer", {})
                          .get("ingress", []))
            if ingress and ingress[0].get("ip"):
                return ingress[0]["ip"]
        return None
    except Exception:
        return None


def resolve_cluster(cluster_name: str) -> dict:
    """Resolve a single cluster by name."""
    import sky  # type: ignore[import-untyped]

    clusters = _materialise(sky.status())

    for cluster in clusters:
        c = _to_mapping(cluster)
        if c.get("name") == cluster_name:
            handle = c.get("handle", {})
            cluster_uid = (getattr(handle, "cluster_name_on_cloud", None)
                           or c.get("cluster_name_on_cloud")
                           or cluster_name)
            # Prefer the LB external IP — that's what the publish ports
            # are reachable on. Fall back to the pod / VM head_ip.
            lb_ip = _kubectl_lb_ip(cluster_uid)
            return {
                "name": cluster_name,
                "ip": (lb_ip
                       or getattr(handle, "head_ip", None)
                       or c.get("head_ip")),
                "ssh_port": getattr(handle, "head_ssh_port", 22),
                "status": _status_str(c.get("status", "UNKNOWN")),
            }

    return {"name": cluster_name, "ip": None, "ssh_port": None, "status": "NOT_FOUND"}


def list_clusters() -> list:
    """List all clusters with relevant fields."""
    clusters = _materialise(__import__("sky").status())
    result = []
    for cluster in clusters:
        c = _to_mapping(cluster)
        handle = c.get("handle", {})
        result.append({
            "name": c.get("name"),
            "status": _status_str(c.get("status", "UNKNOWN")),
            "ip": getattr(handle, "head_ip", None) or c.get("head_ip"),
            "ssh_port": getattr(handle, "head_ssh_port", 22),
            "cloud": c.get("cloud", "unknown"),
            "accelerators": c.get("accelerators", ""),
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
