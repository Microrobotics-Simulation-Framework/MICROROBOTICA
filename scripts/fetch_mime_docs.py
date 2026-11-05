#!/usr/bin/env python3
"""Fetch the pages of a MIME release (0.3 on) for the site to build.

MIME 0.3's documentation is generated: `mime docs build --out <dir>` writes
`index.md` and `pages/` from a MIME checkout and its installed registry, and
MIME's release attaches what its own CI built (with the test lane's reports, so
the evidence counts are read) to the GitHub release as
`mime-docs-<version>.tar.gz`. This script downloads that asset, unpacks it
safely and leaves `index.md` and `pages/` at `--out`, for Sphinx to read as the
MIME project's source. Nothing it writes is committed (`docs/_build/` is
ignored).

The fallback, when the release has no asset (before the release, or to read a
change before it is released): build the pages in a MIME checkout whose
environment has MIME's `verify` extra,

    uv run --no-sync mime docs build --out <dir>

and pass `--from <dir>`. Pages built that way print every evidence count as
"not read" (no lane report), so the site says so until the asset is used.

Usage:
    python3 scripts/fetch_mime_docs.py --tag v0.3.0.dev1 --out docs/_build/mime-pages/0.3.0
    python3 scripts/fetch_mime_docs.py --from <dir> --out docs/_build/mime-pages/0.3.0
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request
from pathlib import Path, PurePosixPath

REPOSITORY = "Microrobotics-Simulation-Framework/MIME"
ASSET = "mime-docs-{version}.tar.gz"
URL = "https://github.com/{repository}/releases/download/{tag}/{asset}"
#: What a build of MIME's pages holds: `mime docs build` writes the marker into `pages/`.
REQUIRED = ("index.md", "pages/errors/index.md")
MARKER = "pages/.mime-docs-build"


class Refused(Exception):
    pass


def asset_url(tag: str, repository: str = REPOSITORY) -> tuple[str, str]:
    version = tag[1:] if tag.startswith("v") else tag
    asset = ASSET.format(version=version)
    return asset, URL.format(repository=repository, tag=tag, asset=asset)


def download(url: str, target: Path) -> str:
    """Download `url` to `target`; its sha256."""
    request = urllib.request.Request(url, headers={"User-Agent": "microrobotica-docs"})
    digest = hashlib.sha256()
    try:
        with urllib.request.urlopen(request, timeout=120) as answer, target.open("wb") as out:
            while chunk := answer.read(1 << 20):
                digest.update(chunk)
                out.write(chunk)
    except urllib.error.HTTPError as error:
        raise Refused(f"{url}: HTTP {error.code}") from error
    except urllib.error.URLError as error:
        raise Refused(f"{url}: {error.reason}") from error
    return digest.hexdigest()


def unpack(archive: Path, into: Path) -> None:
    """Unpack `archive` into `into`, refusing any member that is not a plain
    file or directory below it (an absolute path, `..`, a link, a device)."""
    with tarfile.open(archive, "r:gz") as tar:
        members = tar.getmembers()
        for member in members:
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts:
                raise Refused(f"{archive.name}: member {member.name!r} is outside the archive's directory")
            if not (member.isfile() or member.isdir()):
                raise Refused(f"{archive.name}: member {member.name!r} is not a plain file or directory")
        tar.extractall(into, members=members, filter="data")


def pages_root(directory: Path) -> Path:
    """The directory holding `index.md` and `pages/`: `directory` itself, or the
    one directory it holds (an archive made with a top directory)."""
    if (directory / "index.md").is_file():
        return directory
    children = [child for child in directory.iterdir() if not child.name.startswith(".")]
    if len(children) == 1 and children[0].is_dir() and (children[0] / "index.md").is_file():
        return children[0]
    raise Refused(f"no index.md at the top of the pages (found: {sorted(c.name for c in children)})")


def check_pages(root: Path, where: str) -> int:
    missing = [name for name in REQUIRED if not (root / name).is_file()]
    if missing:
        raise Refused(f"{where} is not a build of MIME's pages: no {', '.join(missing)}")
    if not (root / MARKER).is_file():
        print(f"note: {where} has no {MARKER} (the marker `mime docs build` writes)")
    return sum(1 for _ in (root / "pages").rglob("*.md")) + 1


def place(source: Path, out: Path) -> None:
    """Replace `out` with a copy of the pages at `source` (only `index.md` and `pages/`)."""
    staging = out.with_name(out.name + ".new")
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=True)
    shutil.copy2(source / "index.md", staging / "index.md")
    shutil.copytree(source / "pages", staging / "pages")
    shutil.rmtree(out, ignore_errors=True)
    staging.rename(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    origin = parser.add_mutually_exclusive_group(required=True)
    origin.add_argument("--tag", help="MIME's release tag (v0.3.0.dev1): its asset mime-docs-<version>.tar.gz")
    origin.add_argument("--from", dest="source", type=Path,
                        help="pages already built by `mime docs build --out <dir>` (the fallback)")
    parser.add_argument("--out", type=Path, required=True, help="where index.md and pages/ go")
    parser.add_argument("--sha256", default="", help="the asset's expected sha256, when it is pinned")
    parser.add_argument("--repository", default=REPOSITORY)
    args = parser.parse_args()
    out: Path = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    provenance = out.with_name(out.name + ".source.txt")
    try:
        if args.source is not None:
            source = pages_root(args.source.resolve())
            count = check_pages(source, str(args.source))
            place(source, out)
            said = (f"pages: {count} from a local build (`mime docs build --out {args.source}`), the fallback; "
                    f"not a release asset")
        else:
            asset, url = asset_url(args.tag, args.repository)
            print(f"fetching {url}", flush=True)
            with tempfile.TemporaryDirectory(dir=out.parent, prefix=".fetch-") as held:
                archive = Path(held) / asset
                digest = download(url, archive)
                print(f"{asset}: {archive.stat().st_size} bytes, sha256 {digest}")
                if args.sha256 and digest != args.sha256.lower():
                    raise Refused(f"{asset}: sha256 {digest}, expected {args.sha256}")
                unpacked = Path(held) / "unpacked"
                unpacked.mkdir()
                unpack(archive, unpacked)
                source = pages_root(unpacked)
                count = check_pages(source, asset)
                place(source, out)
            said = f"pages: {count} from {url} (sha256 {digest})"
    except Refused as refusal:
        print(f"fetch_mime_docs: {refusal}", file=sys.stderr)
        if args.tag:
            print("fetch_mime_docs: the release's pages are not there. To build from a MIME checkout instead "
                  "(the fallback): `uv run --no-sync mime docs build --out <dir>` there, then "
                  "`make all-versions MIME_PAGES_DIR=<dir>`.", file=sys.stderr)
        return 1
    provenance.write_text(said + "\n", encoding="utf-8")
    print(f"{said}\n  -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
