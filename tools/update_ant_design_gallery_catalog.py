#!/usr/bin/env python3
"""Generate and verify the pinned Ant Design Gallery catalog without networking."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Callable, Iterable


UPSTREAM_VERSION = "6.5.0"
UPSTREAM_COMMIT = "740ad964dc2397f33e40944367b0536a7314cc32"
EXPECTED_CATEGORY_ORDER = (
    "General",
    "Layout",
    "Navigation",
    "Data Entry",
    "Data Display",
    "Feedback",
    "Other",
)
EXPECTED_CATEGORY_COUNTS = (4, 7, 7, 18, 20, 11, 5)
EXPECTED_DOCUMENT_SOURCES = {
    "ant.document.introduction": "docs/spec/introduce.en-US.md",
    "ant.document.design-values": "docs/spec/values.en-US.md",
    "ant.document.resources": "docs/resources.en-US.md",
    "ant.document.components-overview": "components/overview/index.en-US.md",
}
EXPECTED_INITIAL_PARTIALS = {
    "ant.component.button",
    "ant.component.typography",
    "ant.component.flex",
    "ant.component.space",
    "ant.component.config-provider",
}
ALLOWED_SUPPORT_STATUSES = {
    "implemented",
    "partial",
    "planned",
    "web-only",
    "deprecated",
    "out-of-scope",
}
CATEGORY_ENUMS = {
    "General": "general",
    "Layout": "layout",
    "Navigation": "navigation",
    "Data Entry": "data_entry",
    "Data Display": "data_display",
    "Feedback": "feedback",
    "Other": "other",
}
STATUS_ENUMS = {
    "implemented": "implemented",
    "partial": "partial",
    "planned": "planned",
    "web-only": "web_only",
    "deprecated": "deprecated",
    "out-of-scope": "out_of_scope",
}


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def require_mapping(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def require_list(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ValueError(f"{label} must be an array")
    return value


def require_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def require_keys(value: dict[str, object], keys: Iterable[str], label: str) -> None:
    missing = [key for key in keys if key not in value]
    if missing:
        raise ValueError(f"{label} is missing keys: {', '.join(missing)}")


def validate_schema_contracts(source_schema: object, overlay_schema: object) -> None:
    source = require_mapping(source_schema, "source manifest schema")
    overlay = require_mapping(overlay_schema, "support overlay schema")
    if source.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise ValueError("source manifest schema must use JSON Schema 2020-12")
    source_required = set(require_list(source.get("required"), "source schema required"))
    if source_required != {
        "schema_version", "upstream", "document_sources", "category_order", "categories"
    }:
        raise ValueError("source manifest schema required keys differ from the generator contract")
    overlay_status = (
        require_mapping(overlay.get("properties"), "overlay schema properties")
        .get("entries")
    )
    entries = require_mapping(overlay_status, "overlay entries schema")
    items = require_mapping(entries.get("items"), "overlay entry schema")
    properties = require_mapping(items.get("properties"), "overlay entry properties")
    status = require_mapping(properties.get("status"), "overlay status schema")
    if set(require_list(status.get("enum"), "overlay status enum")) != ALLOWED_SUPPORT_STATUSES:
        raise ValueError("support overlay schema enum differs from the typed status contract")


def validate_manifest(manifest_value: object) -> list[dict[str, object]]:
    manifest = require_mapping(manifest_value, "source manifest")
    require_keys(
        manifest,
        ("schema_version", "upstream", "document_sources", "category_order", "categories"),
        "source manifest",
    )
    if manifest["schema_version"] != 1:
        raise ValueError("source manifest schema version must be 1")
    upstream = require_mapping(manifest["upstream"], "source manifest upstream")
    if upstream.get("version") != UPSTREAM_VERSION or upstream.get("tag") != UPSTREAM_VERSION:
        raise ValueError("source manifest is not locked to Ant Design 6.5.0")
    if upstream.get("commit") != UPSTREAM_COMMIT:
        raise ValueError("source manifest commit differs from the approved snapshot")
    if upstream.get("repository") != "https://github.com/ant-design/ant-design":
        raise ValueError("source manifest repository is not the official Ant Design repository")

    document_sources = require_list(manifest["document_sources"], "document sources")
    actual_document_sources: dict[str, str] = {}
    for index, source_value in enumerate(document_sources):
        source = require_mapping(source_value, f"document source {index}")
        require_keys(
            source,
            ("identity", "title", "chinese_title", "source_path", "chinese_source_path", "official_url"),
            f"document source {index}",
        )
        identity = require_string(source["identity"], f"document source {index} identity")
        actual_document_sources[identity] = require_string(
            source["source_path"], f"document source {identity} path"
        )
        require_string(source["chinese_source_path"], f"document source {identity} Chinese path")
        official_url = require_string(source["official_url"], f"document source {identity} URL")
        if not official_url.startswith("https://ant.design/"):
            raise ValueError(f"document source {identity} does not use an official Ant Design URL")
    if actual_document_sources != EXPECTED_DOCUMENT_SOURCES:
        raise ValueError("Introduction, Design Values, Resources, or Overview source path drifted")

    category_order = tuple(require_list(manifest["category_order"], "category order"))
    if category_order != EXPECTED_CATEGORY_ORDER:
        raise ValueError("Gallery category order differs from the approved Ant Design order")
    categories = require_list(manifest["categories"], "categories")
    if len(categories) != len(EXPECTED_CATEGORY_ORDER):
        raise ValueError("Gallery must contain exactly seven categories")

    flattened: list[dict[str, object]] = []
    identities: set[str] = set()
    english_names: set[str] = set()
    for category_index, (category_value, expected_name, expected_count) in enumerate(
        zip(categories, EXPECTED_CATEGORY_ORDER, EXPECTED_CATEGORY_COUNTS)
    ):
        category = require_mapping(category_value, f"category {category_index}")
        require_keys(
            category,
            ("identity", "name", "chinese_name", "order", "expected_count", "components"),
            f"category {category_index}",
        )
        if category["name"] != expected_name or category["order"] != category_index:
            raise ValueError(f"category {category_index} order or name drifted")
        require_string(category["chinese_name"], f"category {expected_name} Chinese name")
        components = require_list(category["components"], f"category {expected_name} components")
        if category["expected_count"] != expected_count or len(components) != expected_count:
            raise ValueError(f"category {expected_name} must contain {expected_count} entries")
        for entry_index, entry_value in enumerate(components):
            entry = require_mapping(entry_value, f"{expected_name} entry {entry_index}")
            require_keys(
                entry,
                ("identity", "english_name", "chinese_name", "order", "source_path", "chinese_source_path"),
                f"{expected_name} entry {entry_index}",
            )
            identity = require_string(entry["identity"], f"{expected_name} entry identity")
            english_name = require_string(entry["english_name"], f"{identity} English name")
            require_string(entry["chinese_name"], f"{identity} Chinese name")
            source_path = require_string(entry["source_path"], f"{identity} source path")
            chinese_source_path = require_string(
                entry["chinese_source_path"], f"{identity} Chinese source path"
            )
            if not re.fullmatch(r"ant\.component\.[a-z0-9-]+", identity):
                raise ValueError(f"invalid stable component identity: {identity}")
            if not source_path.startswith("components/") or not source_path.endswith("/index.en-US.md"):
                raise ValueError(f"invalid Ant Design 6.5.0 component source path: {source_path}")
            if chinese_source_path != source_path.removesuffix("index.en-US.md") + "index.zh-CN.md":
                raise ValueError(f"Chinese source path differs from English source path: {identity}")
            if entry["order"] != entry_index:
                raise ValueError(f"entry order is not contiguous in category {expected_name}")
            if identity in identities or english_name in english_names:
                raise ValueError(f"duplicate component identity or English name: {identity}")
            identities.add(identity)
            english_names.add(english_name)
            flattened.append({**entry, "category": expected_name})
    if len(flattened) != 72:
        raise ValueError("Gallery source manifest must contain exactly 72 components")
    return flattened


def validate_overlay(
    overlay_value: object,
    source_entries: list[dict[str, object]],
) -> dict[str, dict[str, object]]:
    overlay = require_mapping(overlay_value, "support overlay")
    require_keys(
        overlay,
        ("schema_version", "upstream_version", "upstream_commit", "entries"),
        "support overlay",
    )
    if overlay["schema_version"] != 1:
        raise ValueError("support overlay schema version must be 1")
    if overlay["upstream_version"] != UPSTREAM_VERSION or overlay["upstream_commit"] != UPSTREAM_COMMIT:
        raise ValueError("support overlay does not match the locked Ant Design snapshot")
    overlay_entries = require_list(overlay["entries"], "support overlay entries")
    if len(overlay_entries) != 72:
        raise ValueError("support overlay must contain exactly 72 entries")

    source_identities = [str(entry["identity"]) for entry in source_entries]
    result: dict[str, dict[str, object]] = {}
    for index, entry_value in enumerate(overlay_entries):
        entry = require_mapping(entry_value, f"support overlay entry {index}")
        require_keys(
            entry,
            ("identity", "status", "summary", "supported_scope", "missing_scope", "evidence_identifiers"),
            f"support overlay entry {index}",
        )
        identity = require_string(entry["identity"], f"support overlay entry {index} identity")
        if identity in result:
            raise ValueError(f"duplicate support overlay identity: {identity}")
        status = require_string(entry["status"], f"{identity} status")
        if status not in ALLOWED_SUPPORT_STATUSES:
            raise ValueError(f"invalid GallerySupportStatus for {identity}: {status}")
        require_string(entry["summary"], f"{identity} summary")
        supported = require_list(entry["supported_scope"], f"{identity} supported scope")
        missing = require_list(entry["missing_scope"], f"{identity} missing scope")
        evidence = require_list(entry["evidence_identifiers"], f"{identity} evidence")
        for label, values in (("supported scope", supported), ("missing scope", missing), ("evidence", evidence)):
            if not values or any(not isinstance(item, str) or not item.strip() for item in values):
                raise ValueError(f"{identity} {label} must contain non-empty strings")
        if status in {"implemented", "partial"}:
            if not any(str(item).startswith("openspec:") for item in evidence):
                raise ValueError(f"{identity} {status} status requires completed OpenSpec evidence")
            if not any(str(item).startswith("test:") for item in evidence):
                raise ValueError(f"{identity} {status} status requires automated test evidence")
            if any("planning" in str(item).lower() for item in evidence):
                raise ValueError(f"{identity} cannot use planning-only evidence")
        result[identity] = entry
    if list(result) != source_identities:
        raise ValueError("support overlay identities or order differ from the source manifest")
    for identity in EXPECTED_INITIAL_PARTIALS:
        if result[identity]["status"] != "partial":
            raise ValueError(f"initial supported subset must remain explicit partial: {identity}")
    return result


def canonical_json(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def cpp_string(value: object) -> str:
    return json.dumps(str(value), ensure_ascii=False)


def joined(values: object) -> str:
    return " · ".join(str(item) for item in require_list(values, "generated string list"))


def render_metadata(
    manifest: dict[str, object],
    source_entries: list[dict[str, object]],
    overlay: dict[str, dict[str, object]],
    catalog_hash: str,
) -> bytes:
    lines = [
        "// Generated by tools/update_ant_design_gallery_catalog.py. Do not edit.",
        f"constexpr std::string_view kAntDesignReferenceCatalogHash = {cpp_string(catalog_hash)};",
        f"constexpr std::string_view kAntDesignReferenceVersion = {cpp_string(UPSTREAM_VERSION)};",
        f"constexpr std::string_view kAntDesignReferenceCommit = {cpp_string(UPSTREAM_COMMIT)};",
        "constexpr std::array<AntDesignReferenceSource, 4> kAntDesignReferenceSources{{",
    ]
    for source_value in require_list(manifest["document_sources"], "document sources"):
        source = require_mapping(source_value, "document source")
        lines.append(
            "    AntDesignReferenceSource{"
            f"{cpp_string(source['identity'])}, {cpp_string(source['title'])}, "
            f"{cpp_string(source['chinese_title'])}, {cpp_string(source['source_path'])}, "
            f"{cpp_string(source['chinese_source_path'])}, "
            f"{cpp_string(source.get('renderer_source_path', ''))}, "
            f"{cpp_string(source['official_url'])}"
            "},"
        )
    lines.extend(("}};", "constexpr std::array<AntDesignReferenceCategory, 7> kAntDesignReferenceCategories{{"))
    for category_value in require_list(manifest["categories"], "categories"):
        category = require_mapping(category_value, "category")
        lines.append(
            "    AntDesignReferenceCategory{"
            f"AntDesignGalleryCategory::{CATEGORY_ENUMS[str(category['name'])]}, "
            f"{cpp_string(category['identity'])}, {cpp_string(category['name'])}, "
            f"{cpp_string(category['chinese_name'])}, {category['order']}, {category['expected_count']}"
            "},"
        )
    lines.extend(("}};", "constexpr std::array<AntDesignReferenceEntry, 72> kAntDesignReferenceEntries{{"))
    for entry in source_entries:
        status = overlay[str(entry["identity"])]
        lines.append(
            "    AntDesignReferenceEntry{"
            f"{cpp_string(entry['identity'])}, {cpp_string(entry['english_name'])}, "
            f"{cpp_string(entry['chinese_name'])}, "
            f"AntDesignGalleryCategory::{CATEGORY_ENUMS[str(entry['category'])]}, "
            f"{cpp_string(entry['source_path'])}, {cpp_string(entry['chinese_source_path'])}, "
            f"GallerySupportStatus::{STATUS_ENUMS[str(status['status'])]}, "
            f"{cpp_string(status['summary'])}, {cpp_string(joined(status['supported_scope']))}, "
            f"{cpp_string(joined(status['missing_scope']))}, "
            f"{cpp_string(joined(status['evidence_identifiers']))}"
            "},"
        )
    lines.extend(("}};", ""))
    return "\n".join(lines).encode("utf-8")


def input_paths(repo_root: Path) -> tuple[Path, Path, Path, Path]:
    root = repo_root / "gallery/ant-design"
    version_root = root / UPSTREAM_VERSION
    return (
        version_root / "source-manifest.json",
        version_root / "support-overlay.json",
        root / "source-manifest.schema.json",
        root / "support-overlay.schema.json",
    )


def output_path(repo_root: Path) -> Path:
    return repo_root / "examples/token_gallery/generated_ant_design_reference_catalog.inc"


def build_output(repo_root: Path) -> bytes:
    manifest_path, overlay_path, source_schema_path, overlay_schema_path = input_paths(repo_root)
    manifest_value = load_json(manifest_path)
    overlay_value = load_json(overlay_path)
    source_schema = load_json(source_schema_path)
    overlay_schema = load_json(overlay_schema_path)
    validate_schema_contracts(source_schema, overlay_schema)
    source_entries = validate_manifest(manifest_value)
    overlay = validate_overlay(overlay_value, source_entries)
    inputs = {
        "manifest": manifest_value,
        "overlay": overlay_value,
        "source_schema": source_schema,
        "overlay_schema": overlay_schema,
    }
    catalog_hash = hashlib.sha256(canonical_json(inputs)).hexdigest()
    return render_metadata(
        require_mapping(manifest_value, "source manifest"),
        source_entries,
        overlay,
        catalog_hash,
    )


def write_output(repo_root: Path) -> None:
    target = output_path(repo_root)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(build_output(repo_root))


def check_output(repo_root: Path) -> None:
    target = output_path(repo_root)
    expected = build_output(repo_root)
    if not target.is_file():
        raise ValueError(f"generated Gallery catalog is stale: {target}")
    verify_output_bytes(target.read_bytes(), expected, target)


def verify_output_bytes(actual: bytes, expected: bytes, target: object = "generated output") -> None:
    if actual != expected:
        raise ValueError(f"generated Gallery catalog is stale: {target}")


def validate_no_network_imports(script_text: str) -> None:
    forbidden = re.compile(r"(?m)^\s*(?:from|import)\s+(?:aiohttp|http|requests|socket|urllib)\b")
    if forbidden.search(script_text):
        raise ValueError("offline Gallery catalog generator imports a network module")


def expect_invalid(action: Callable[[], object], label: str) -> None:
    try:
        action()
    except (ValueError, KeyError, TypeError):
        return
    raise AssertionError(f"invalid catalog fixture was accepted: {label}")


def self_test(repo_root: Path) -> None:
    manifest_path, overlay_path, source_schema_path, overlay_schema_path = input_paths(repo_root)
    manifest = require_mapping(load_json(manifest_path), "source manifest")
    overlay = require_mapping(load_json(overlay_path), "support overlay")
    validate_schema_contracts(load_json(source_schema_path), load_json(overlay_schema_path))
    source_entries = validate_manifest(manifest)
    validate_overlay(overlay, source_entries)
    expected_output = build_output(repo_root)
    validate_no_network_imports(Path(__file__).read_text(encoding="utf-8"))
    expect_invalid(
        lambda: verify_output_bytes(b"stale output\n", expected_output),
        "stale generated output",
    )

    invalid_status = copy.deepcopy(overlay)
    invalid_status["entries"][0]["status"] = "unknown"
    expect_invalid(lambda: validate_overlay(invalid_status, source_entries), "status enum")

    duplicate = copy.deepcopy(manifest)
    duplicate["categories"][0]["components"][1]["identity"] = duplicate["categories"][0]["components"][0]["identity"]
    expect_invalid(lambda: validate_manifest(duplicate), "duplicate identity")

    wrong_version = copy.deepcopy(manifest)
    wrong_version["upstream"]["version"] = "6.6.2"
    expect_invalid(lambda: validate_manifest(wrong_version), "source version")

    wrong_order = copy.deepcopy(manifest)
    wrong_order["category_order"][0], wrong_order["category_order"][1] = (
        wrong_order["category_order"][1], wrong_order["category_order"][0]
    )
    expect_invalid(lambda: validate_manifest(wrong_order), "category order")

    missing_evidence = copy.deepcopy(overlay)
    button = next(item for item in missing_evidence["entries"] if item["identity"] == "ant.component.button")
    button["evidence_identifiers"] = ["planning-only"]
    expect_invalid(lambda: validate_overlay(missing_evidence, source_entries), "support evidence gate")


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if not any((args.write, args.check, args.self_test)):
        parser.error("select --write, --check, or --self-test")
    try:
        repo_root = args.repo_root.resolve()
        if args.write:
            write_output(repo_root)
        if args.check:
            check_output(repo_root)
        if args.self_test:
            self_test(repo_root)
    except (OSError, ValueError, KeyError, TypeError, AssertionError, json.JSONDecodeError) as error:
        print(f"Ant Design Gallery catalog error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
