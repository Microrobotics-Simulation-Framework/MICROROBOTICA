#!/usr/bin/env python3
"""Mirror the canonical docs/glossary.md into MADDENING and MIME.

The unified docs site builds three projects under one Sphinx config but the
`{term}` role is local to each project — so each repo carries its own copy
of glossary.md. This script copies the MICROROBOTICA canonical version into
the sibling repos, dereferencing the symlinks under docs/projects/.

Idempotent. Prints a status line per target.

Usage:
    python3 scripts/sync_glossary.py [--check]

With --check, exits non-zero if any mirror is out of date — useful in CI.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DOCS = HERE.parent / "docs"
CANONICAL = DOCS / "glossary.md"
MIRRORS = [
    DOCS / "projects" / "maddening" / "docs" / "glossary.md",
    DOCS / "projects" / "mime" / "docs" / "glossary.md",
]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if any mirror is out of sync.",
    )
    args = parser.parse_args()

    if not CANONICAL.is_file():
        print(f"FATAL: canonical glossary missing at {CANONICAL}", file=sys.stderr)
        return 2

    src_hash = sha256(CANONICAL)
    drift = 0
    for target in MIRRORS:
        # Resolve through the projects/ symlink so we land in the real sibling repo.
        real = target.resolve() if target.exists() else target
        if real.is_file() and sha256(real) == src_hash:
            print(f"ok    {real} (sha {src_hash[:8]})")
            continue
        if args.check:
            print(f"drift {real}", file=sys.stderr)
            drift += 1
            continue
        real.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CANONICAL, real)
        print(f"wrote {real}")

    if args.check and drift:
        print(f"\n{drift} mirror(s) out of sync — run without --check to fix.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
