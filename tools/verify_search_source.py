#!/usr/bin/env python3
"""Verify the pinned Search source contract without network access."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHANGE = ROOT / "openspec/changes/014-20260925-compose-search-from-input-and-button"
SOURCE_ROOT = ROOT / "out/upstream-ant-design-6.6.5"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify() -> None:
    contract = json.loads((CHANGE / "evidence/source-contract.json").read_text(encoding="utf-8"))
    manifest = json.loads((ROOT / "gallery/ant-design/6.6.5/source-manifest.json").read_text(encoding="utf-8"))
    source_diff = json.loads((ROOT / "openspec/changes/012-20260922-upgrade-ant-design-reference-to-6-6-5/evidence/source-diff.json").read_text(encoding="utf-8"))
    version = contract["upstream_version"]
    commit = contract["upstream_commit"]
    require(version == manifest["upstream"]["version"] == source_diff["upstream"]["new_version"],
            "Search version differs from pinned reference")
    require(commit == manifest["upstream"]["commit"] == source_diff["upstream"]["new_commit"],
            "Search commit differs from pinned reference")
    diff_sources = {entry["path"]: entry for entry in source_diff["sources"]}
    for path, record in contract["sources"].items():
        require(len(record["sha256"]) == 64 and len(record["git_blob"]) == 40,
                f"invalid Search digest: {path}")
        if path in diff_sources:
            require(diff_sources[path]["sha256_665"] == record["sha256"],
                    f"Search source differs from change 012: {path}")
        local = SOURCE_ROOT / path
        if local.is_file():
            require(hashlib.sha256(local.read_bytes()).hexdigest() == record["sha256"],
                    f"local Search source differs: {path}")
            blob = subprocess.check_output(
                ["git", "-C", str(SOURCE_ROOT), "rev-parse", f"{commit}:{path}"],
                text=True, stderr=subprocess.PIPE).strip()
            require(blob == record["git_blob"], f"Search blob differs: {path}")

    require(set(contract["sources"]) == {
        "components/input/Search.tsx", "components/input/style/search.ts",
        "components/input/index.en-US.md", "components/input/demo/search-input.tsx",
        "components/input/demo/search-input-loading.tsx",
    }, "Search source set changed")
    require(contract["behavior"] == {
        "submit_source": "input",
        "enter_during_composition": "blocked",
        "enter_during_loading": "blocked",
        "button_during_loading": "blocked",
        "button_while_composing": "last-committed-value",
    }, "Search behavior matrix changed")
    require("allowClear" in contract["not_supported"] and "onSearch" in contract["supported"],
            "Search support boundary is missing")
    source = SOURCE_ROOT / "components/input/Search.tsx"
    docs = SOURCE_ROOT / "components/input/index.en-US.md"
    if source.is_file() and docs.is_file():
        search = source.read_text(encoding="utf-8")
        document = docs.read_text(encoding="utf-8")
        for fragment in ("source: 'input'", "composedRef.current || loading", "<Button", "<Input"):
            require(fragment in search, f"Search upstream behavior missing: {fragment}")
        for prop in ("enterButton", "loading", "onSearch", "searchIcon"):
            require(f"| {prop} |" in document, f"Search upstream doc missing: {prop}")


if __name__ == "__main__":
    try:
        verify()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Search source contract failed: {error}", file=sys.stderr)
        raise SystemExit(1)
    print("Search source contract passed")
