#!/usr/bin/env python3
"""Reject mixed Ant Design versions in the current generated inputs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import update_ant_design_gallery_catalog as gallery
import update_ant_design_tokens as tokens


def require_same_identity(*pairs: tuple[str, str]) -> None:
    if len(set(pairs)) != 1:
        raise ValueError(f"mixed Ant Design current versions: {pairs}")


def require_current_docs(repo: Path) -> None:
    markers = {
        "README.md": ("Ant Design 6.6.5", "七类 73 项"),
        "README.en.md": ("Ant Design 6.6.5", "73 entries"),
        "docs/architecture.md": ("当前参考版本为 `6.6.5`", "1198 个 Token"),
        "docs/design-tokens.md": ("Ant Design 6.6.5 Design Token 规范", "Token 总数：`1198`"),
        "examples/token_gallery/gallery_document_model.cpp": ("Ant Design 6.6.5", "七类 73 项"),
    }
    for path, expected in markers.items():
        content = (repo / path).read_text(encoding="utf-8")
        if any(marker not in content for marker in expected):
            raise ValueError(f"current Ant Design documentation drifted: {path}")


def check(repo: Path) -> None:
    tokens.verify_repository(repo)
    gallery.check_output(repo)
    token_root = repo / "design-tokens/ant-design" / tokens.UPSTREAM_VERSION
    gallery_root = repo / "gallery/ant-design" / gallery.UPSTREAM_VERSION
    lock = json.loads((token_root / "sources.lock.yaml").read_text(encoding="utf-8"))
    manifest = json.loads((gallery_root / "source-manifest.json").read_text(encoding="utf-8"))
    overlay = json.loads((gallery_root / "support-overlay.json").read_text(encoding="utf-8"))
    catalog = json.loads((token_root / "catalog.yaml").read_text(encoding="utf-8"))
    require_same_identity(
        (tokens.UPSTREAM_VERSION, tokens.UPSTREAM_COMMIT),
        (gallery.UPSTREAM_VERSION, gallery.UPSTREAM_COMMIT),
        (lock["upstream"]["version"], lock["upstream"]["commit"]),
        (catalog["upstream"]["version"], catalog["upstream"]["commit"]),
        (manifest["upstream"]["version"], manifest["upstream"]["commit"]),
        (overlay["upstream_version"], overlay["upstream_commit"]),
    )
    if tokens.UPSTREAM_VERSION != "6.6.5":
        raise ValueError("current Ant Design version is not the approved 6.6.5 release")
    if catalog["coverage"]["entries"] != 1198:
        raise ValueError("current Token count differs from the reviewed source")
    if sum(category["expected_count"] for category in manifest["categories"]) != 73:
        raise ValueError("current Gallery count differs from the reviewed source")
    require_current_docs(repo)


def self_test() -> None:
    require_same_identity(("6.6.5", "commit"), ("6.6.5", "commit"))
    for bad in (("6.5.0", "commit"), ("6.6.5", "other")):
        try:
            require_same_identity(("6.6.5", "commit"), bad)
        except ValueError:
            pass
        else:
            raise AssertionError("mixed Ant Design identity was accepted")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    check(args.repo_root.resolve())
    print("Ant Design 6.6.5 current baseline: PASS")
