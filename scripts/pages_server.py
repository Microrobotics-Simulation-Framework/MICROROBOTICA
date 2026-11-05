#!/usr/bin/env python3
"""Serve a built site the way GitHub Pages resolves a URL, to preview and to test it.

`python3 -m http.server` resolves a URL differently from GitHub Pages: it never
serves `x.html` for `/x`, so an extensionless link (every MIME refusal's `see:`
line, `https://microrobotica.org/mime/0.3.0/pages/errors/<code>`) cannot be
checked with it. This server applies the rules below, each one read from
microrobotica.org itself (served by GitHub Pages, `server: GitHub.com`) on
2026-11-04:

1. A path naming a file is that file:
   `/maddening/release_notes/v0.3.0.html` is 200.
2. A path ending in `/` is the directory's `index.html`, or 404 when there is
   none; it is never `<name>.html`: `/maddening/release_notes/` is 200 (its
   index), `/maddening/release_notes/v0.3.0/` is 404 although `v0.3.0.html`
   exists.
3. Otherwise, a path with `.html` appended that names a file is that file,
   served with no redirect, whatever dots the last segment holds:
   `/maddening/release_notes/v0.3.0` and `/mime/release_notes/v0.2` are 200,
   byte for byte the `.html` page, as `text/html`.
4. Otherwise, a path naming a directory is a 301 to the same path with `/`:
   `/maddening/v0.4.2` and `/mime/v0.2` are 301 to `/maddening/v0.4.2/` and
   `/mime/v0.2/`.
5. Anything else is 404 (the site's `404.html` when it has one).

The query string is ignored, and a file's media type is read from its own
extension. Where a page `x.html` and a directory `x/` both exist, rule 3 comes
before rule 4 here (the file wins); the live site has no such pair, so that
order is not observed.

Usage:
    python3 scripts/pages_server.py docs/_build/html [--port 8000] [--bind 127.0.0.1]
"""
from __future__ import annotations

import argparse
import http.server
import mimetypes
import posixpath
import sys
import threading
import urllib.parse
from pathlib import Path


def resolve(root: Path, url_path: str) -> tuple[int, Path | str | None]:
    """How GitHub Pages answers `url_path` over the site at `root`: `(200, file)`,
    `(301, location)` or `(404, the site's 404.html or None)`."""
    path = urllib.parse.unquote(urllib.parse.urlsplit(url_path).path) or "/"
    if not path.startswith("/"):
        path = "/" + path
    parts = [p for p in path.split("/") if p not in ("", ".")]
    if ".." in parts:
        return 404, None
    target = root.joinpath(*parts)
    not_found = root / "404.html"
    missing = (404, not_found if not_found.is_file() else None)
    if path.endswith("/"):  # rule 2
        index = target / "index.html"
        return (200, index) if index.is_file() else missing
    if target.is_file():  # rule 1
        return 200, target
    page = target.with_name(target.name + ".html")
    if page.is_file():  # rule 3
        return 200, page
    if target.is_dir():  # rule 4
        return 301, posixpath.normpath(path) + "/"
    return missing  # rule 5


class PagesHandler(http.server.BaseHTTPRequestHandler):
    root: Path
    quiet = False

    def _answer(self, body: bool) -> None:
        status, found = resolve(self.root, self.path)
        if status == 301:
            assert isinstance(found, str)
            self.send_response(301)
            self.send_header("Location", found)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if isinstance(found, Path):
            data = found.read_bytes()
            kind = mimetypes.guess_type(found.name)[0] or "application/octet-stream"
        else:
            data, kind = b"<!DOCTYPE html><title>Page not found</title>\n", "text/html"
        if kind.startswith("text/") or kind in ("application/javascript", "application/json"):
            kind += "; charset=utf-8"
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if body:
            self.wfile.write(data)

    def do_GET(self) -> None:  # noqa: N802 (http.server's naming)
        self._answer(body=True)

    def do_HEAD(self) -> None:  # noqa: N802
        self._answer(body=False)

    def log_message(self, format: str, *args: object) -> None:  # noqa: A002
        if not self.quiet:
            super().log_message(format, *args)


def serve(root: Path, bind: str = "127.0.0.1", port: int = 0, *, quiet: bool = False,
          background: bool = False) -> http.server.ThreadingHTTPServer:
    """A server for the site at `root`; `port` 0 takes a free one. With
    `background`, it runs in a daemon thread and is returned at once."""
    handler = type("Handler", (PagesHandler,), {"root": root.resolve(), "quiet": quiet})
    server = http.server.ThreadingHTTPServer((bind, port), handler)
    if background:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("root", type=Path, help="the built site (docs/_build/html)")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--bind", default="127.0.0.1")
    args = parser.parse_args()
    if not args.root.is_dir():
        print(f"{args.root} is not a directory", file=sys.stderr)
        return 2
    server = serve(args.root, args.bind, args.port)
    print(f"Serving {args.root} with GitHub Pages' rules at http://{args.bind}:{server.server_port}/",
          flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
