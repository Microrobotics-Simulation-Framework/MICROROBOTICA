#!/usr/bin/env python3
"""Validate that all [@Key] citations in docs/ resolve to bibliography.bib entries.

Template files (*_template.md) are excluded from checking.
"""

import re
import sys
from pathlib import Path

DOCS_DIR = Path(__file__).parent.parent / "docs"
BIB_FILE = DOCS_DIR / "bibliography.bib"
CITATION_PATTERN = re.compile(r'\[@(\w+)\]')


def parse_bib_keys(bib_path: Path) -> set:
    """Extract @type{key, entries from a .bib file."""
    if not bib_path.exists():
        return set()
    keys = set()
    with open(bib_path) as f:
        for line in f:
            match = re.match(r'@\w+\{(\w+),', line)
            if match:
                keys.add(match.group(1))
    return keys


def find_citations(docs_dir: Path) -> dict:
    """Find all [@Key] citations in .md files, excluding templates."""
    citations = {}  # key -> list of (file, line_number)
    for md_file in docs_dir.rglob("*.md"):
        if "_template" in md_file.name:
            continue
        with open(md_file) as f:
            for line_num, line in enumerate(f, 1):
                for match in CITATION_PATTERN.finditer(line):
                    key = match.group(1)
                    citations.setdefault(key, []).append((md_file, line_num))
    return citations


def main():
    bib_keys = parse_bib_keys(BIB_FILE)
    citations = find_citations(DOCS_DIR)

    errors = []
    for key, locations in citations.items():
        if key not in bib_keys:
            for filepath, line_num in locations:
                rel = filepath.relative_to(DOCS_DIR.parent)
                errors.append(f"  [@{key}] in {rel}:{line_num} — not in bibliography.bib")

    if errors:
        print(f"FAIL: {len(errors)} unresolved citation(s):")
        for e in errors:
            print(e)
        return 1

    total = sum(len(v) for v in citations.values())
    print(f"OK: {total} citation(s) checked, all resolved ({len(bib_keys)} bib entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
