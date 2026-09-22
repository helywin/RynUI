#!/usr/bin/env python3
"""Reproduce the Ant Design 6.5.0 -> 6.6.5 source audit offline."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import update_ant_design_tokens as tokens


OLD_VERSION = "6.5.0"
NEW_VERSION = "6.6.5"
OLD_COMMIT = "740ad964dc2397f33e40944367b0536a7314cc32"
NEW_COMMIT = "4a39f54842eade4e565ab336ef6097cd7e723cdd"
FOCUS_PATHS = (
    "components/button/Button.tsx",
    "components/button/buttonHelpers.tsx",
    "components/button/style/index.ts",
    "components/button/style/variant.ts",
    "components/input/index.tsx",
    "components/input/style/variants.ts",
    "components/input/style/search.ts",
    "components/input/style/textarea.ts",
    "components/checkbox/Checkbox.tsx",
    "components/checkbox/style/index.ts",
    "components/switch/index.tsx",
    "components/switch/style/index.ts",
    "components/theme/themes/seed.ts",
    "components/theme/util/alias.ts",
)


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True, stderr=subprocess.PIPE
    ).strip()


def digest(path: Path) -> str | None:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def validate_records(records: list[dict[str, object]]) -> None:
    paths = [str(row["path"]) for row in records]
    if len(paths) != len(set(paths)):
        raise ValueError("duplicate audited source path")
    for row in records:
        old_hash, new_hash = row["sha256_650"], row["sha256_665"]
        if old_hash is None and new_hash is None:
            raise ValueError(f"missing source in both releases: {row['path']}")
        for value in (old_hash, new_hash):
            if value is not None and (not isinstance(value, str) or len(value) != 64
                                      or any(char not in "0123456789abcdef" for char in value)):
                raise ValueError(f"invalid SHA256: {row['path']}")
        expected = "added" if old_hash is None else "removed" if new_hash is None else "equal" if old_hash == new_hash else "changed"
        if row["status"] != expected:
            raise ValueError(f"invalid source status: {row['path']}")


def self_test() -> None:
    valid = [{"path": "a", "status": "equal", "sha256_650": "0" * 64,
              "sha256_665": "0" * 64}]
    validate_records(valid)
    cases = (
        valid + valid,
        [{"path": "a", "status": "removed", "sha256_650": None, "sha256_665": None}],
        [{"path": "a", "status": "equal", "sha256_650": "0" * 63, "sha256_665": "0" * 64}],
        [{"path": "a", "status": "changed", "sha256_650": "0" * 64, "sha256_665": "0" * 64}],
    )
    for rows in cases:
        try:
            validate_records(rows)
        except ValueError:
            continue
        raise AssertionError("invalid source record was accepted")


def source_paths(value: object) -> set[str]:
    found: set[str] = set()
    if isinstance(value, dict):
        for key, child in value.items():
            if key.endswith("source_path") and isinstance(child, str):
                found.add(child)
            else:
                found.update(source_paths(child))
    elif isinstance(value, list):
        for child in value:
            found.update(source_paths(child))
    return found


def frontmatter_group(path: Path) -> str | None:
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8")
    if not text.startswith("---\n"):
        return None
    lines = text.split("---", 2)[1].splitlines()
    for index, line in enumerate(lines):
        if line.startswith("group: "):
            return line.partition(":")[2].strip()
        if line == "group:" and index + 1 < len(lines) and lines[index + 1].startswith("  title:"):
            return lines[index + 1].partition(":")[2].strip()
    return None


def build(repo: Path, old: Path, new: Path) -> dict[str, object]:
    for version, root, commit in (
        (OLD_VERSION, old, OLD_COMMIT), (NEW_VERSION, new, NEW_COMMIT)
    ):
        if git(root, "rev-parse", "HEAD") != commit:
            raise ValueError(f"{version} checkout commit mismatch")
        if git(root, "rev-parse", f"refs/tags/{version}^{{commit}}") != commit:
            raise ValueError(f"{version} tag commit mismatch")
        if not (root / "LICENSE").read_text(encoding="utf-8").startswith("MIT LICENSE\n"):
            raise ValueError(f"{version} license is not MIT")

    lock = json.loads((repo / "design-tokens/ant-design/6.5.0/sources.lock.yaml").read_text(encoding="utf-8"))
    gallery = json.loads((repo / "gallery/ant-design/6.5.0/source-manifest.json").read_text(encoding="utf-8"))
    if lock["upstream"]["commit"] != OLD_COMMIT or gallery["upstream"]["commit"] != OLD_COMMIT:
        raise ValueError("historical inputs have an unexpected commit")
    locked = {row["path"]: row["sha256"] for row in lock["sources"]}
    if len(locked) != len(lock["sources"]):
        raise ValueError("duplicate historical source path")
    for path, expected in locked.items():
        if digest(old / path) != expected:
            raise ValueError(f"historical source hash mismatch: {path}")

    old_catalog, old_token_paths = tokens.build_catalog(old)
    new_catalog, new_token_paths = tokens.build_catalog(new)
    old_tokens = {row["identity"]: row for row in old_catalog["entries"]}
    new_tokens = {row["identity"]: row for row in new_catalog["entries"]}
    if len(old_tokens) != 1194 or len(old_tokens) != len(old_catalog["entries"]):
        raise ValueError("historical token inventory changed")
    if len(new_tokens) != len(new_catalog["entries"]):
        raise ValueError("duplicate new token identity")

    paths = set(locked) | source_paths(gallery) | set(FOCUS_PATHS)
    paths.update(path.relative_to(old).as_posix() for path in old_token_paths)
    paths.update(path.relative_to(new).as_posix() for path in new_token_paths)
    paths.update(path.relative_to(new).as_posix() for path in (new / "components").glob("*/index.en-US.md"))
    records = []
    for path in sorted(paths):
        old_hash, new_hash = digest(old / path), digest(new / path)
        if old_hash is None and new_hash is None:
            raise ValueError(f"missing source in both releases: {path}")
        status = "added" if old_hash is None else "removed" if new_hash is None else "equal" if old_hash == new_hash else "changed"
        records.append({"path": path, "status": status, "sha256_650": old_hash, "sha256_665": new_hash})
    validate_records(records)

    def token_record(identity: str) -> dict[str, object]:
        before, after = old_tokens.get(identity), new_tokens.get(identity)
        status = "added" if before is None else "removed" if after is None else "equal"
        changed: list[str] = []
        if before and after:
            for field in ("upstream_type", "upstream_default", "value_kind", "support", "deprecated", "internal", "component_owner"):
                if before[field] != after[field]:
                    changed.append(field)
            if changed:
                status = "changed"
        return {
            "identity": identity, "status": status, "changed_fields": changed,
            "source_650": before["source"]["path"] if before else None,
            "source_665": after["source"]["path"] if after else None,
            "default_650": before["upstream_default"] if before else None,
            "default_665": after["upstream_default"] if after else None,
        }

    token_records = [token_record(identity) for identity in sorted(old_tokens.keys() | new_tokens.keys())]
    old_components = {row["identity"]: (category["name"], row["source_path"])
                      for category in gallery["categories"] for row in category["components"]}
    component_records = []
    for identity, (old_category, path) in sorted(old_components.items()):
        if not (new / path).is_file():
            status = "removed"
        else:
            status = "equal" if digest(old / path) == digest(new / path) else "changed"
        component_records.append({"identity": identity, "status": status, "category_650": old_category,
                                  "category_665": frontmatter_group(new / path), "source_path": path})
    old_component_paths = {path for _, path in old_components.values()} | source_paths(gallery["document_sources"])
    new_component_docs = sorted(path.relative_to(new).as_posix()
                                for path in (new / "components").glob("*/index.en-US.md"))
    added_component_docs = sorted(set(new_component_docs) - old_component_paths)
    return {
        "schema_version": 1,
        "upstream": {"repository": "https://github.com/ant-design/ant-design", "license": "MIT",
                     "old_version": OLD_VERSION, "old_commit": OLD_COMMIT,
                     "new_version": NEW_VERSION, "new_commit": NEW_COMMIT},
        "coverage": {"source_paths": len(records), "token_650": len(old_tokens), "token_665": len(new_tokens),
                     "gallery_650": len(component_records), "new_component_doc_candidates": len(added_component_docs)},
        "new_component_doc_candidates": added_component_docs,
        "sources": records,
        "tokens": token_records,
        "gallery_components": component_records,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old-source", required=True, type=Path)
    parser.add_argument("--new-source", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.check == args.write:
        parser.error("select exactly one of --check or --write")
    if args.self_test:
        self_test()
    repo = Path(__file__).resolve().parents[1]
    result = build(repo, args.old_source.resolve(), args.new_source.resolve())
    output = repo / "openspec/changes/012-20260922-upgrade-ant-design-reference-to-6-6-5/evidence/source-diff.json"
    data = (json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if args.write:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    elif output.read_bytes() != data:
        raise ValueError("source audit differs from checked-in evidence")
    print(json.dumps(result["coverage"], sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Ant Design source audit error: {error}", file=sys.stderr)
        sys.exit(1)
