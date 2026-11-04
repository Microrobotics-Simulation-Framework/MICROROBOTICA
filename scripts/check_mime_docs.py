#!/usr/bin/env python3
"""Check the links of a MIME release's pages (0.3 on): built, and deployed.

Every refusal MIME 0.3 prints ends with a link to its code's page,
`https://microrobotica.org/mime/<release>/pages/errors/<code>`, extensionless
(`mime.build.page_of`). Two checks hold those links:

`build` (the docs workflow, after `make all-versions`): the codes are the ones
MIME's generated catalogue lists (`pages/errors/index.md`, which
`mime docs build` writes from MIME's refusal table), and each one has its
`pages/errors/<code>.md`; each has a built page under `/mime/<release>/` and
under the `/mime/` alias; each extensionless URL answers 200 with that page
from a server that resolves URLs as GitHub Pages does (`pages_server.py`); and
every link inside the release's built pages, anchor included, lands on a page
of the build. It writes a manifest of the codes for the second check.

`live` (the docs workflow, after the deploy): a sample of the extensionless
URLs answers 200 with the right page on the live site, retried while the CDN
may still hold an earlier answer.

Usage:
    python3 scripts/check_mime_docs.py build --pages docs/_build/mime-pages/0.3.0 \
        --site docs/_build/html --version 0.3.0 [--refusal-table FILE] [--manifest FILE]
    python3 scripts/check_mime_docs.py live --manifest FILE [--base https://microrobotica.org]
"""
from __future__ import annotations

import argparse
import html
import html.parser
import json
import mimetypes
import posixpath
import random
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

sys.dont_write_bytecode = True  # no scripts/__pycache__ from the import below
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pages_server  # noqa: E402

#: A code's row in the generated catalogue: ``- [`asset.conflict`](asset.conflict.md)``.
_INDEX_ROW = re.compile(r"^- \[`([^`]+)`\]\(([^)\s]+)\.md\)", re.MULTILINE)
_TITLE = re.compile(r"<title>(.*?)</title>", re.DOTALL)
#: The codes the brief and MIME's own examples name, sampled on every live check.
ALWAYS = ("bundle.referenced-copy", "asset.conflict", "setting.value")


def catalogue(pages: Path) -> list[str]:
    index = pages / "pages" / "errors" / "index.md"
    rows = _INDEX_ROW.findall(index.read_text(encoding="utf-8"))
    wrong = [f"{code} -> {target}.md" for code, target in rows if code != target]
    if wrong:
        raise SystemExit(f"FAIL: {index}: a row links another page than its code's: {wrong[:5]}")
    return sorted({code for code, _ in rows})


def refusal_table(path: Path) -> set[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    return {line.split()[0] for line in lines if line.strip() and not line.startswith("#")}


def title_of(text: str) -> str:
    found = _TITLE.search(text)
    return html.unescape(found.group(1)).strip() if found else ""


def page_names_code(text: str, code: str) -> bool:
    """A code's built page is titled `The error <code>`."""
    return f"The error {code}" in title_of(text)


def suffix_is_a_media_type(code: str) -> bool:
    """A code whose last part reads as a file extension a server knows (`asset.manifest`, `live.step`)."""
    return mimetypes.guess_type("x/" + code, strict=False)[0] is not None


class _Links(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.hrefs: list[str] = []
        self.ids: set[str] = set()

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for name, value in attrs:
            if value is None:
                continue
            if name == "id" or (name == "name" and tag == "a"):
                self.ids.add(value)
            elif name == "href" and tag in ("a", "area"):
                self.hrefs.append(value)


def _parsed(path: Path, cache: dict[Path, _Links]) -> _Links:
    if path not in cache:
        parser = _Links()
        parser.feed(path.read_text(encoding="utf-8", errors="replace"))
        cache[path] = parser
    return cache[path]


def internal_links(site: Path, release: Path, deploy_base: str) -> tuple[int, list[str]]:
    """Every link of the pages under `release` that lands inside the site (relative,
    root-relative, or an absolute link to `deploy_base`): its page exists, as
    GitHub Pages resolves it, and so does its anchor."""
    cache: dict[Path, _Links] = {}
    checked, broken = 0, []
    base = urllib.parse.urlsplit(deploy_base)
    for page in sorted(release.rglob("*.html")):
        page_url = "/" + page.relative_to(site).as_posix()
        for href in _parsed(page, cache).hrefs:
            parts = urllib.parse.urlsplit(href)
            if parts.scheme in ("http", "https"):
                if (parts.scheme, parts.netloc) != (base.scheme, base.netloc):
                    continue
                path = parts.path or "/"
            elif parts.scheme or parts.netloc or href in ("", "#"):
                continue  # another host or scheme (mailto:), or no target
            elif not parts.path:
                path = page_url  # "#anchor" on the page itself
            else:
                path = parts.path if parts.path.startswith("/") else posixpath.join(
                    posixpath.dirname(page_url), parts.path)
                path = posixpath.normpath(path) + ("/" if parts.path.endswith("/") else "")
            checked += 1
            status, found = pages_server.resolve(site, path)
            while status == 301 and isinstance(found, str):
                status, found = pages_server.resolve(site, found)
            where = f"{page.relative_to(site)}: {href}"
            if status != 200 or not isinstance(found, Path):
                broken.append(f"{where} (no page)")
            elif parts.fragment and found.suffix == ".html":
                anchor = urllib.parse.unquote(parts.fragment)
                if anchor not in _parsed(found, cache).ids:
                    broken.append(f"{where} (no anchor #{anchor})")
    return checked, broken


def _get(url: str, timeout: float = 30) -> tuple[int, str, str]:
    request = urllib.request.Request(url, headers={"User-Agent": "microrobotica-docs-check"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as answer:
            return answer.status, answer.headers.get("Content-Type", ""), answer.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as error:
        return error.code, error.headers.get("Content-Type", ""), ""
    except (urllib.error.URLError, TimeoutError) as error:
        return 0, "", str(error)


def build(args: argparse.Namespace) -> int:
    pages: Path = args.pages
    site: Path = args.site.resolve()
    version: str = args.version
    failures: list[str] = []

    codes = catalogue(pages)
    if not codes:
        print(f"FAIL: {pages}/pages/errors/index.md lists no code")
        return 1
    files = sorted(p.stem for p in (pages / "pages" / "errors").glob("*.md") if p.name != "index.md")
    print(f"codes: {len(codes)} listed in the catalogue (pages/errors/index.md), "
          f"{len(files)} pages/errors/<code>.md")
    for code in sorted(set(codes) - set(files)):
        failures.append(f"{code}: listed, and no pages/errors/{code}.md")
    for code in sorted(set(files) - set(codes)):
        failures.append(f"{code}: pages/errors/{code}.md, and not listed in the catalogue")
    if args.refusal_table:
        table = refusal_table(args.refusal_table)
        print(f"codes: {len(table)} in the refusal table {args.refusal_table.name}")
        for code in sorted(table - set(codes)):
            failures.append(f"{code}: in the refusal table, and not in the catalogue")
        for code in sorted(set(codes) - table):
            failures.append(f"{code}: in the catalogue, and not in the refusal table")

    prefixes = [f"/mime/{version}/"] + ([] if args.no_latest else ["/mime/"])
    for prefix in prefixes:
        root = site / prefix.strip("/")
        absent = [c for c in codes if not (root / "pages" / "errors" / f"{c}.html").is_file()]
        for code in absent:
            failures.append(f"{prefix}pages/errors/{code}.html: not built")
        print(f"built: {len(codes) - len(absent)} of {len(codes)} under {prefix}pages/errors/")

    server = pages_server.serve(site, quiet=True, background=True)
    local = f"http://127.0.0.1:{server.server_port}"
    try:
        for prefix in prefixes:
            served = 0
            for code in codes:
                url = f"{local}{prefix}pages/errors/{code}"
                status, kind, body = _get(url)
                if status != 200 or not kind.startswith("text/html") or not page_names_code(body, code):
                    failures.append(f"{prefix}pages/errors/{code}: {status} {kind!r} "
                                    f"title {title_of(body)!r} (extensionless, GitHub Pages' rules)")
                else:
                    served += 1
            for path in (f"{prefix}", f"{prefix}pages/errors/", f"{prefix}pages/errors/index"):
                status, _, _ = _get(local + path)
                if status != 200:
                    failures.append(f"{path}: {status}")
            print(f"served: {served} of {len(codes)} extensionless {prefix}pages/errors/<code> "
                  f"answer 200 with the code's page (GitHub Pages' rules)")
    finally:
        server.shutdown()

    checked, broken = internal_links(site, site / "mime" / version, args.deploy_base)
    print(f"links: {checked} inside /mime/{version}/'s pages, {len(broken)} broken")
    failures += [f"link {b}" for b in broken]

    if args.manifest:
        args.manifest.write_text(json.dumps({"version": version, "codes": codes}, indent=1) + "\n",
                                 encoding="utf-8")
        print(f"manifest: {len(codes)} codes -> {args.manifest}")
    if failures:
        print(f"FAIL: {len(failures)} problem(s):")
        for failure in failures[:200]:
            print(f"  {failure}")
        return 1
    print("ok")
    return 0


def live(args: argparse.Namespace) -> int:
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    version, codes = manifest["version"], manifest["codes"]
    base = args.base.rstrip("/")
    known = set(codes)
    always = [c for c in ALWAYS if c in known] + [c for c in codes if suffix_is_a_media_type(c)]
    rest = [c for c in codes if c not in always]
    sample = list(dict.fromkeys(always + random.Random(args.seed).sample(rest, min(args.sample, len(rest)))))
    checks: list[tuple[str, str | None]] = [(f"/mime/{version}/pages/errors/{c}", c) for c in sample]
    checks += [(f"/mime/pages/errors/{c}", c) for c in sample[:3]]
    checks += [(p, None) for p in (f"/mime/{version}/", f"/mime/{version}/pages/errors/", "/mime/",
                                   "/mime/v0.2/", "/mime/v0.1/")]
    print(f"live: {len(checks)} URLs on {base} ({len(sample)} of {len(codes)} codes, seed {args.seed}; "
          f"always: {', '.join(always)})")
    pending = dict(checks)
    deadline = time.monotonic() + args.patience
    while True:
        for path, code in list(pending.items()):
            status, kind, body = _get(base + path)
            good = status == 200 and kind.startswith("text/html") and (code is None or page_names_code(body, code))
            if good:
                print(f"  200 {path}")
                del pending[path]
            else:
                pending[path] = code
                print(f"  {status or 'error'} {path} {title_of(body)!r}")
        if not pending or time.monotonic() > deadline:
            break
        print(f"  {len(pending)} not yet right; again in {args.interval} s (the CDN may hold an earlier answer)")
        time.sleep(args.interval)
    status, _, body = _get(f"{base}/mime/_static/switcher.json")
    try:
        listed = [row["version"] for row in json.loads(body)] if status == 200 else []
    except (ValueError, KeyError, TypeError):
        listed = []
    print(f"  switcher: {status} {listed}")
    if version not in listed:
        pending[f"/mime/_static/switcher.json lists {version}"] = None
    if pending:
        print(f"FAIL: {len(pending)} URL(s) not answered with their page: {sorted(pending)}")
        return 1
    print("ok")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="mode", required=True)
    b = sub.add_parser("build", help="check a local build")
    b.add_argument("--pages", type=Path, required=True, help="the release's pages (index.md, pages/)")
    b.add_argument("--site", type=Path, required=True, help="the built site (docs/_build/html)")
    b.add_argument("--version", required=True, help="the release's path label (0.3.0)")
    b.add_argument("--refusal-table", type=Path, help="MIME's src/mime/build/refusal_table.txt, to compare")
    b.add_argument("--no-latest", action="store_true", help="do not check the /mime/ alias")
    b.add_argument("--deploy-base", default="https://microrobotica.org",
                   help="absolute links to this host are checked against the build")
    b.add_argument("--manifest", type=Path, help="write the codes here, for `live`")
    b.set_defaults(run=build)
    v = sub.add_parser("live", help="sample the deployed site")
    v.add_argument("--manifest", type=Path, required=True)
    v.add_argument("--base", default="https://microrobotica.org")
    v.add_argument("--sample", type=int, default=30, help="codes sampled beside the ones always checked")
    v.add_argument("--seed", type=int, default=int(time.time()) // 86400)
    v.add_argument("--patience", type=float, default=720, help="seconds to keep retrying a wrong answer")
    v.add_argument("--interval", type=float, default=30)
    v.set_defaults(run=live)
    args = parser.parse_args()
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
