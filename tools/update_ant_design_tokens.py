#!/usr/bin/env python3
"""Import and verify the pinned Ant Design token inventory without networking."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


UPSTREAM_NAME = "Ant Design"
UPSTREAM_VERSION = "6.5.0"
UPSTREAM_TAG = "6.5.0"
UPSTREAM_COMMIT = "740ad964dc2397f33e40944367b0536a7314cc32"
EXPECTED_SOURCE_SET_SHA256 = "08bc2917bb7fe1b809756f1eabf1b9841a1478373437037ebe52b7a6a9431ef6"
PRESET_COLORS = (
    "blue", "purple", "cyan", "green", "magenta", "pink", "red",
    "orange", "yellow", "volcano", "geekblue", "lime", "gold",
)
RUNTIME_GLOBALS = {
    "colorPrimary", "colorSuccess", "colorWarning", "colorError", "colorInfo",
    "colorLink", "colorTextBase", "colorBgBase", "fontFamily", "fontFamilyCode",
    "fontSize", "lineWidth", "lineType", "borderRadius", "sizeUnit", "sizeStep",
    "sizePopupArrow", "controlHeight", "zIndexBase", "zIndexPopupBase",
    "opacityImage", "motionUnit", "motionBase", "motion", "wireframe",
    "colorText", "colorTextSecondary", "colorTextTertiary", "colorTextQuaternary",
    "colorTextDisabled", "colorBgContainer", "colorBgContainerDisabled",
    "colorBorder", "colorBorderSecondary", "colorPrimaryHover", "colorPrimaryActive",
    "colorPrimaryBorder", "colorErrorOutline", "controlOutline", "controlTmpOutline",
    "controlOutlineWidth", "lineWidthFocus", "boxShadow", "boxShadowSecondary",
    "boxShadowTertiary", "boxShadowPopoverArrow", "dropShadowPopover",
    "boxShadowCard", "boxShadowDrawerRight", "boxShadowDrawerLeft",
    "boxShadowDrawerUp", "boxShadowDrawerDown", "boxShadowTabsOverflowLeft",
    "boxShadowTabsOverflowRight", "boxShadowTabsOverflowTop",
    "boxShadowTabsOverflowBottom", "fontSizeSM", "fontSizeLG", "lineHeight",
    "lineHeightSM", "lineHeightLG", "controlHeightSM", "controlHeightLG",
    "paddingContentHorizontal", "marginXS", "opacityLoading",
}
RUNTIME_BUTTONS = {
    "fontWeight", "iconGap", "defaultShadow", "primaryShadow", "dangerShadow",
    "primaryColor", "defaultColor", "defaultBg", "defaultBorderColor", "dangerColor",
    "defaultHoverBg", "defaultHoverColor", "defaultHoverBorderColor",
    "defaultActiveBg", "defaultActiveColor", "defaultActiveBorderColor",
    "borderColorDisabled", "paddingInline", "paddingInlineLG", "paddingInlineSM",
    "contentFontSize", "contentFontSizeLG", "contentFontSizeSM", "contentLineHeight",
    "contentLineHeightLG", "contentLineHeightSM", "defaultBgDisabled",
}
SEED_DEFAULTS = {
    "blue": "#1677FF", "purple": "#722ED1", "cyan": "#13C2C2",
    "green": "#52C41A", "magenta": "#EB2F96", "pink": "#EB2F96",
    "red": "#F5222D", "orange": "#FA8C16", "yellow": "#FADB14",
    "volcano": "#FA541C", "geekblue": "#2F54EB", "gold": "#FAAD14",
    "lime": "#A0D911", "colorPrimary": "#1677ff", "colorSuccess": "#52c41a",
    "colorWarning": "#faad14", "colorError": "#ff4d4f", "colorInfo": "#1677ff",
    "colorLink": "", "colorTextBase": "", "colorBgBase": "",
    "fontFamily": "system-ui web stack", "fontFamilyCode": "system monospace web stack",
    "fontSize": 14, "lineWidth": 1, "lineType": "solid", "motionUnit": 0.1,
    "motionBase": 0, "motionEaseOutCirc": "cubic-bezier(0.08, 0.82, 0.17, 1)",
    "motionEaseInOutCirc": "cubic-bezier(0.78, 0.14, 0.15, 0.86)",
    "motionEaseOut": "cubic-bezier(0.215, 0.61, 0.355, 1)",
    "motionEaseInOut": "cubic-bezier(0.645, 0.045, 0.355, 1)",
    "motionEaseOutBack": "cubic-bezier(0.12, 0.4, 0.29, 1.46)",
    "motionEaseInBack": "cubic-bezier(0.71, -0.46, 0.88, 0.6)",
    "motionEaseInQuint": "cubic-bezier(0.755, 0.05, 0.855, 0.06)",
    "motionEaseOutQuint": "cubic-bezier(0.23, 1, 0.32, 1)",
    "borderRadius": 6, "sizeUnit": 4, "sizeStep": 4, "sizePopupArrow": 16,
    "controlHeight": 32, "zIndexBase": 0, "zIndexPopupBase": 1000,
    "opacityImage": 1, "wireframe": False, "motion": True,
}
CATALOG_SCHEMA = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": "rynui://design-tokens/ant-design/catalog.schema.json",
    "title": "RynUI Ant Design token catalog",
    "type": "object",
    "required": ["schema_version", "upstream", "coverage", "component_owners", "entries"],
    "properties": {
        "schema_version": {"const": 1},
        "upstream": {"type": "object", "required": ["name", "version", "commit"]},
        "coverage": {
            "type": "object",
            "required": ["entries", "missing", "duplicates", "unclassified", "by_layer", "by_support"],
        },
        "component_owners": {"type": "array", "items": {"type": "string"}, "uniqueItems": True},
        "empty_components": {"type": "array", "items": {"type": "string"}, "uniqueItems": True},
        "entries": {
            "type": "array",
            "items": {
                "type": "object",
                "required": [
                    "identity", "upstream_name", "layer", "category", "value_kind",
                    "upstream_type", "source", "support", "ryn_mapping",
                    "invalidation_domain", "deprecated", "internal",
                ],
            },
        },
    },
}


@dataclass(frozen=True)
class Field:
    name: str
    type_text: str
    path: str
    line: int
    deprecated: bool = False
    internal: bool = False


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode()


def source_set_sha256(records: object) -> str:
    data = json.dumps(
        records, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return sha256_bytes(data)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def matching_brace(text: str, opening: int) -> int:
    depth = 0
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    index = opening
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            index += 1
            continue
        if char == "/" and next_char == "/":
            line_comment = True
            index += 2
            continue
        if char == "/" and next_char == "*":
            block_comment = True
            index += 2
            continue
        if char in "'\"`":
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError("unbalanced TypeScript block")


FIELD_PATTERN = re.compile(
    r"(?P<doc>/\*\*.*?\*/\s*)?(?:readonly\s+)?(?P<name>[A-Za-z_$][\w$]*)(?:\?)?\s*:\s*(?P<type>[^;{}]+);",
    re.DOTALL,
)


def fields_in_block(text: str, opening: int, path: str) -> list[Field]:
    closing = matching_brace(text, opening)
    body = text[opening + 1:closing]
    body = re.sub(
        r"(?m)//.*$",
        lambda match: " " * len(match.group(0)),
        body,
    )
    base_line = text[:opening].count("\n") + 1
    fields: list[Field] = []
    for match in FIELD_PATTERN.finditer(body):
        prefix = body[:match.start("name")]
        line = base_line + prefix.count("\n")
        doc = match.group("doc") or ""
        fields.append(Field(
            match.group("name"),
            " ".join(match.group("type").split()),
            path,
            line,
            "@deprecated" in doc,
            "@internal" in doc or "@private" in doc,
        ))
    return fields


def named_interface_fields(text: str, name: str, path: str) -> list[Field]:
    pattern = re.compile(
        r"(?:export\s+)?interface\s+" + re.escape(name) + r"\b[^{]*\{")
    result: list[Field] = []
    for match in pattern.finditer(text):
        result.extend(fields_in_block(text, match.end() - 1, path))
    type_pattern = re.compile(rf"(?:export\s+)?type\s+{re.escape(name)}\s*=")
    for match in type_pattern.finditer(text):
        semicolon = text.find(";", match.end())
        search_end = semicolon if semicolon >= 0 else len(text)
        opening = text.find("{", match.end(), search_end)
        if opening >= 0:
            result.extend(fields_in_block(text, opening, path))
    return result


def all_interface_fields(path: Path, root: Path) -> list[Field]:
    text = read_text(path)
    result: list[Field] = []
    for match in re.finditer(
        r"(?:export\s+)?interface\s+([A-Za-z_$][\w$]*)\b[^{]*\{", text
    ):
        result.extend(fields_in_block(text, match.end() - 1, relative(root, path)))
    return result


def components_config(source_root: Path) -> list[tuple[str, Path]]:
    path = source_root / "components/theme/interface/components.ts"
    text = read_text(path)
    imports: dict[str, str] = {}
    for match in re.finditer(
        r"import\s+type\s+\{\s*ComponentToken\s+as\s+(\w+)\s*\}\s+from\s+'([^']+)'", text
    ):
        imports[match.group(1)] = match.group(2)
    interface_match = re.search(r"export\s+interface\s+ComponentTokenMap\s*{", text)
    if interface_match is None:
        raise ValueError("ComponentTokenMap was not found")
    closing = matching_brace(text, interface_match.end() - 1)
    body = text[interface_match.end():closing]
    result: list[tuple[str, Path]] = []
    for match in re.finditer(r"^\s*(\w+)\?\s*:\s*(\w+)\s*;", body, re.MULTILINE):
        owner, alias = match.groups()
        import_path = imports.get(alias)
        if import_path is None:
            raise ValueError(f"missing import for component token alias {alias}")
        candidate = (path.parent / import_path).resolve()
        result.append((owner, candidate))
    return result


def imported_types(path: Path) -> dict[str, tuple[str, str]]:
    text = read_text(path)
    result: dict[str, tuple[str, str]] = {}
    for match in re.finditer(
        r"import\s+type\s+\{(?P<names>.*?)\}\s+from\s+'(?P<path>[^']+)'",
        text,
        re.DOTALL,
    ):
        for item in match.group("names").split(","):
            parts = item.strip().split()
            if not parts:
                continue
            if len(parts) == 3 and parts[1] == "as":
                original, local = parts[0], parts[2]
            else:
                original = local = parts[0]
            result[local] = (original, match.group("path"))
    return result


def definition_references(text: str, name: str) -> list[str]:
    references: list[str] = []
    interface = re.search(
        r"(?:export\s+)?interface\s+" + re.escape(name) + r"\b(?P<header>[^{]*)\{",
        text,
    )
    if interface is not None and "extends" in interface.group("header"):
        extends = interface.group("header").split("extends", 1)[1]
        references.extend(re.findall(r"\b[A-Za-z_$][\w$]*Token\b", extends))
    alias = re.search(
        r"(?:export\s+)?type\s+" + re.escape(name) + r"\s*=\s*(?P<body>.*?);",
        text,
        re.DOTALL,
    )
    if alias is not None:
        references.extend(re.findall(r"\b[A-Za-z_$][\w$]*Token\b", alias.group("body")))
    return [reference for reference in references if reference != name]


def resolve_type_source(base: Path, import_path: str, type_name: str) -> Path | None:
    target = (base.parent / import_path).resolve()
    candidates: list[Path] = []
    if target.is_dir():
        candidates.extend(sorted(target.rglob("*.ts")))
        candidates.extend(sorted(target.rglob("*.tsx")))
    else:
        candidates.extend((target.with_suffix(".ts"), target.with_suffix(".tsx"), target))
    definition = re.compile(r"(?:interface|type)\s+" + re.escape(type_name) + r"\b")
    for candidate in candidates:
        if candidate.is_file() and definition.search(read_text(candidate)):
            return candidate
    return None


def collect_type_fields(
    source_root: Path,
    path: Path,
    type_name: str,
    visited: set[tuple[Path, str]],
    definition_paths: set[Path],
) -> list[Field]:
    key = (path, type_name)
    if key in visited:
        return []
    visited.add(key)
    text = read_text(path)
    if not re.search(r"(?:interface|type)\s+" + re.escape(type_name) + r"\b", text):
        return []
    definition_paths.add(path)
    result = named_interface_fields(text, type_name, relative(source_root, path))
    local_imports = imported_types(path)
    excluded = {"AliasToken", "FullToken", "GlobalToken", "SeedToken", "MapToken"}
    for reference in definition_references(text, type_name):
        if reference in excluded:
            continue
        if re.search(r"(?:interface|type)\s+" + re.escape(reference) + r"\b", text):
            result.extend(collect_type_fields(
                source_root, path, reference, visited, definition_paths))
            continue
        imported = local_imports.get(reference)
        if imported is None:
            continue
        original, import_path = imported
        imported_source = resolve_type_source(path, import_path, original)
        if imported_source is not None:
            result.extend(collect_type_fields(
                source_root, imported_source, original, visited, definition_paths))
    return result


def component_fields(
    source_root: Path, owner: str, source: Path
) -> tuple[list[Field], list[Path]]:
    candidates: list[Path]
    if source.is_dir():
        candidates = sorted(source.rglob("*.ts")) + sorted(source.rglob("*.tsx"))
    else:
        candidates = [source.with_suffix(".ts"), source.with_suffix(".tsx"), source]
    seen_paths: set[Path] = set()
    definition_paths: set[Path] = set()
    fields: list[Field] = []
    for candidate in candidates:
        if candidate in seen_paths or not candidate.is_file():
            continue
        seen_paths.add(candidate)
        text = read_text(candidate)
        if re.search(r"(?:export\s+)?(?:interface|type)\s+ComponentToken\b", text):
            fields.extend(collect_type_fields(
                source_root, candidate, "ComponentToken", set(), definition_paths))
    unique: dict[str, Field] = {}
    for field in fields:
        unique.setdefault(field.name, field)
    if not definition_paths:
        raise ValueError(f"no ComponentToken definition discovered for {owner} at {source}")
    return list(unique.values()), sorted(definition_paths)


def category(name: str) -> str:
    lower = name.lower()
    checks = (
        ("shadow", "shadow"), ("color", "color"), ("bg", "color"),
        ("font", "typography"), ("lineheight", "typography"),
        ("padding", "spacing"), ("margin", "spacing"), ("gap", "spacing"),
        ("radius", "radius"), ("border", "border"), ("line", "border"),
        ("motion", "motion"), ("duration", "motion"), ("ease", "motion"),
        ("opacity", "opacity"), ("zindex", "z-index"), ("screen", "breakpoint"),
        ("height", "size"), ("width", "size"), ("size", "size"),
        ("image", "image"), ("focus", "focus"), ("outline", "focus"),
    )
    for fragment, value in checks:
        if fragment in lower:
            return value
    return "component" if ".component." in name else "misc"


def value_kind(field: Field, token_category: str) -> str:
    type_text = field.type_text
    if token_category == "shadow":
        return "shadow-list"
    if token_category == "color":
        return "color"
    if "boolean" in type_text:
        return "boolean"
    if "number" in type_text:
        if token_category == "motion":
            return "duration"
        if token_category == "z-index":
            return "integer"
        return "logical-length"
    if "CSSProperties" in type_text or "React." in type_text:
        return "web-css-value"
    if token_category == "motion":
        return "cubic-bezier"
    if "string" in type_text:
        return "string"
    return "typed-expression"


def invalidation_domain(token_category: str) -> str:
    return {
        "color": "paint-material", "opacity": "paint-material",
        "shadow": "effect-geometry-material", "focus": "effect-geometry-material",
        "typography": "text-measure-layout", "spacing": "measure-layout-hittest",
        "size": "measure-layout-hittest", "breakpoint": "measure-layout-hittest",
        "radius": "geometry-paint", "border": "geometry-paint",
        "motion": "animation", "z-index": "paint-order",
    }.get(token_category, "metadata")


def support(field: Field, layer: str, owner: str | None, kind: str) -> str:
    if field.deprecated:
        return "deprecated"
    if kind == "web-css-value":
        return "web-only"
    if layer == "component" and owner != "Button":
        return "component-not-yet-implemented"
    if layer == "component":
        return "runtime" if field.name in RUNTIME_BUTTONS else "metadata"
    return "runtime" if field.name in RUNTIME_GLOBALS else "metadata"


def entry(field: Field, layer: str, owner: str | None = None) -> dict[str, object]:
    identity = f"ant.{layer}.{field.name}" if owner is None else f"ant.component.{owner}.{field.name}"
    token_category = category(identity)
    kind = value_kind(field, token_category)
    classification = support(field, layer, owner, kind)
    upstream_default = SEED_DEFAULTS.get(field.name) if layer == "seed" else None
    return {
        "identity": identity,
        "upstream_name": field.name,
        "layer": layer,
        "category": token_category,
        "value_kind": kind,
        "upstream_type": field.type_text,
        "upstream_default": upstream_default,
        "derivation": None if upstream_default is not None else f"source:{field.path}:{field.line}",
        "source": {"path": field.path, "line": field.line},
        "support": classification,
        "ryn_mapping": identity if classification == "runtime" else "catalog-only",
        "invalidation_domain": invalidation_domain(token_category),
        "component_owner": owner,
        "deprecated": field.deprecated,
        "internal": field.internal,
        "adaptation": (
            "使用平台 system UI font token，保留上游 Web font stack 作为来源记录"
            if field.name == "fontFamily" else
            "解析为 typed desktop value；runtime 不解析 CSS 字符串"
            if kind in {"shadow-list", "web-css-value"} else None
        ),
    }


def build_catalog(source_root: Path) -> tuple[dict[str, object], list[Path]]:
    interface_root = source_root / "components/theme/interface"
    seed_path = interface_root / "seeds.ts"
    alias_path = interface_root / "alias.ts"
    preset_path = interface_root / "presetColors.ts"
    fields: list[dict[str, object]] = []
    sources: set[Path] = {seed_path, alias_path, preset_path, source_root / "components/theme/themes/seed.ts"}

    seed_fields = named_interface_fields(read_text(seed_path), "SeedToken", relative(source_root, seed_path))
    existing_seed = {field.name for field in seed_fields}
    preset_line = 1
    for preset in PRESET_COLORS:
        if preset not in existing_seed:
            seed_fields.append(Field(preset, "string", relative(source_root, preset_path), preset_line))
    fields.extend(entry(field, "seed") for field in seed_fields)

    map_fields: list[Field] = []
    for path in sorted((interface_root / "maps").glob("*.ts")):
        sources.add(path)
        map_fields.extend(all_interface_fields(path, source_root))
    preset_names = [f"{color}{index}" for color in PRESET_COLORS for index in range(1, 11)]
    legacy_names = [f"{color}-{index}" for color in PRESET_COLORS for index in range(1, 11)]
    map_fields.extend(Field(name, "string", relative(source_root, preset_path), 21) for name in preset_names)
    map_fields.extend(Field(name, "string", relative(source_root, preset_path), 23, deprecated=True) for name in legacy_names)
    fields.extend(entry(field, "map") for field in map_fields)

    alias_fields = named_interface_fields(read_text(alias_path), "AliasToken", relative(source_root, alias_path))
    fields.extend(entry(field, "alias") for field in alias_fields)

    components_path = interface_root / "components.ts"
    sources.add(components_path)
    configured_components = components_config(source_root)
    component_owners = [owner for owner, _ in configured_components]
    empty_components: list[str] = []
    for owner, source in configured_components:
        discovered, definition_paths = component_fields(source_root, owner, source)
        if not discovered:
            empty_components.append(owner)
        fields.extend(entry(field, "component", owner) for field in discovered)
        sources.update(definition_paths)

    by_identity: dict[str, dict[str, object]] = {}
    duplicates: list[str] = []
    for item in fields:
        identity = str(item["identity"])
        if identity in by_identity:
            duplicates.append(identity)
        else:
            by_identity[identity] = item
    if duplicates:
        raise ValueError("duplicate token identities: " + ", ".join(sorted(set(duplicates))))
    entries = [by_identity[key] for key in sorted(by_identity)]
    counts: dict[str, int] = {}
    support_counts: dict[str, int] = {}
    for item in entries:
        counts[str(item["layer"])] = counts.get(str(item["layer"]), 0) + 1
        support_counts[str(item["support"])] = support_counts.get(str(item["support"]), 0) + 1
    catalog = {
        "schema_version": 1,
        "upstream": {"name": UPSTREAM_NAME, "version": UPSTREAM_VERSION, "commit": UPSTREAM_COMMIT},
        "coverage": {
            "entries": len(entries), "missing": 0, "duplicates": 0, "unclassified": 0,
            "component_owners": len(component_owners),
            "by_layer": counts, "by_support": support_counts,
        },
        "component_owners": sorted(component_owners),
        "empty_components": sorted(empty_components),
        "entries": entries,
    }
    return catalog, sorted(sources)


def render_document(catalog: dict[str, object], catalog_hash: str) -> str:
    coverage = catalog["coverage"]
    entries = catalog["entries"]
    lines = [
        "# RynUI Ant Design 6.5.0 Design Token 规范", "",
        "> 此文件由 `tools/update_ant_design_tokens.py` 生成。请勿手工修改。", "",
        f"- 上游：Ant Design `{UPSTREAM_VERSION}` / `{UPSTREAM_COMMIT}`",
        f"- Catalog SHA256：`{catalog_hash}`",
        f"- Token 总数：`{coverage['entries']}`；missing/duplicate/unclassified 均为 `0`。", "",
        "## 使用规则", "",
        "稳定组件的颜色、排版、尺寸、间距、边框、圆角、阴影、动效与交互状态只允许消费 Theme/Component Token。", 
        "`catalog.yaml` 保存完整上游名录；`support=runtime` 表示已有 typed runtime mapping，其他分类仍是后续组件的唯一设计输入。",
        "Upstream value、normalized typed value 与 RynUI desktop adaptation 必须分别记录；runtime 不解析 CSS shorthand。", "",
        "## 完整性摘要", "",
        "| Layer | Count |", "| --- | ---: |",
    ]
    for layer, count in sorted(coverage["by_layer"].items()):
        lines.append(f"| `{layer}` | {count} |")
    lines.extend(["", "## 分类摘要", "", "| Support | Count |", "| --- | ---: |"])
    for name, count in sorted(coverage["by_support"].items()):
        lines.append(f"| `{name}` | {count} |")
    lines.extend(["", "## Token 索引", "", "| Identity | Category | Kind | Support | Source |", "| --- | --- | --- | --- | --- |"])
    for item in entries:
        source = item["source"]
        lines.append(
            f"| `{item['identity']}` | `{item['category']}` | `{item['value_kind']}` | "
            f"`{item['support']}` | `{source['path']}:{source['line']}` |"
        )
    lines.extend(["", "## 升级流程", "", "切换上游版本必须创建独立 OpenSpec change，先生成 added/removed/renamed/default/derivation/classification diff，再更新 runtime 与视觉证据。", ""])
    return "\n".join(lines)


def git_commit(source_root: Path) -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=source_root, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    return result.stdout.strip()


def stable_token_id(identity: str) -> int:
    value = 14695981039346656037
    for byte in identity.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def render_token_metadata(catalog: dict[str, object]) -> bytes:
    kind_names = {
        "boolean": "boolean", "color": "color", "cubic-bezier": "cubic_bezier",
        "duration": "duration", "integer": "integer", "logical-length": "logical_length",
        "shadow-list": "shadow_list", "string": "string", "typed-expression": "typed_expression",
        "web-css-value": "web_css_value",
    }
    support_names = {
        "runtime": "runtime", "metadata": "metadata", "web-only": "web_only",
        "deprecated": "deprecated",
        "component-not-yet-implemented": "component_not_yet_implemented",
    }
    domain_names = {
        "animation": "animation", "effect-geometry-material": "effect_geometry_material",
        "geometry-paint": "geometry_paint", "measure-layout-hittest": "measure_layout_hittest",
        "metadata": "metadata", "paint-material": "paint_material", "paint-order": "paint_order",
        "text-measure-layout": "text_measure_layout",
    }
    lines = ["// Generated by tools/update_ant_design_tokens.py. Do not edit."]
    seen_ids: dict[int, str] = {}
    for entry in catalog["entries"]:
        identity = str(entry["identity"])
        stable_id = stable_token_id(identity)
        if stable_id in seen_ids:
            raise ValueError(
                f"stable token identity collision: {identity} and {seen_ids[stable_id]}")
        seen_ids[stable_id] = identity
        owner = entry["component_owner"] or ""
        lines.append(
            "TokenMetadata{"
            f"0x{stable_id:016x}ULL, {json.dumps(identity)}, "
            f"TokenValueKind::{kind_names[entry['value_kind']]}, {json.dumps(owner)}, "
            f"TokenSupportStatus::{support_names[entry['support']]}, "
            f"TokenInvalidationDomain::{domain_names[entry['invalidation_domain']]}"
            "},"
        )
    return ("\n".join(lines) + "\n").encode("utf-8")


def build_outputs(source_root: Path) -> tuple[bytes, bytes, bytes, bytes, bytes]:
    if git_commit(source_root) != UPSTREAM_COMMIT:
        raise ValueError("Ant Design source checkout is not at the locked commit")
    catalog, source_paths = build_catalog(source_root)
    catalog_data = json_bytes(catalog)
    schema_data = json_bytes(CATALOG_SCHEMA)
    document_data = render_document(catalog, sha256_bytes(catalog_data)).encode("utf-8")
    metadata_data = render_token_metadata(catalog)
    license_path = source_root / "LICENSE"
    source_paths.append(license_path)
    source_records = [
        {"path": relative(source_root, path), "sha256": sha256_bytes(path.read_bytes())}
        for path in sorted(set(source_paths))
    ]
    if source_set_sha256(source_records) != EXPECTED_SOURCE_SET_SHA256:
        raise ValueError("pinned Ant Design source set differs from the approved manifest")
    lock = {
        "schema_version": 1,
        "upstream": {
            "name": UPSTREAM_NAME, "version": UPSTREAM_VERSION, "tag": UPSTREAM_TAG,
            "commit": UPSTREAM_COMMIT, "license": "MIT", "license_path": "LICENSE",
        },
        "sources": source_records,
        "schema": {"path": "../catalog.schema.json", "sha256": sha256_bytes(schema_data)},
        "catalog": {"path": "catalog.yaml", "sha256": sha256_bytes(catalog_data), "entries": catalog["coverage"]["entries"]},
        "document": {"path": "../../../docs/design-tokens.md", "sha256": sha256_bytes(document_data)},
        "generated_metadata": {
            "path": "../../../src/theme/generated_token_metadata.inc",
            "sha256": sha256_bytes(metadata_data),
            "entries": catalog["coverage"]["entries"],
        },
    }
    return json_bytes(lock), catalog_data, schema_data, document_data, metadata_data


def output_paths(repo_root: Path) -> tuple[Path, Path, Path, Path, Path]:
    token_root = repo_root / "design-tokens/ant-design/6.5.0"
    return (
        token_root / "sources.lock.yaml",
        token_root / "catalog.yaml",
        repo_root / "design-tokens/ant-design/catalog.schema.json",
        repo_root / "docs/design-tokens.md",
        repo_root / "src/theme/generated_token_metadata.inc",
    )


def write_outputs(repo_root: Path, outputs: tuple[bytes, bytes, bytes, bytes, bytes]) -> None:
    for path, data in zip(output_paths(repo_root), outputs):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def verify_repository(repo_root: Path) -> None:
    lock_path, catalog_path, schema_path, document_path, metadata_path = output_paths(repo_root)
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    catalog_data = catalog_path.read_bytes()
    schema_data = schema_path.read_bytes()
    document_data = document_path.read_bytes()
    metadata_data = metadata_path.read_bytes()
    catalog = json.loads(catalog_data)
    if lock["upstream"]["commit"] != UPSTREAM_COMMIT:
        raise ValueError("source lock commit is not the approved Ant Design commit")
    if source_set_sha256(lock["sources"]) != EXPECTED_SOURCE_SET_SHA256:
        raise ValueError("source file paths or SHA256 values differ from the approved set")
    if sha256_bytes(catalog_data) != lock["catalog"]["sha256"]:
        raise ValueError("catalog SHA256 differs from sources.lock.yaml")
    if sha256_bytes(schema_data) != lock["schema"]["sha256"]:
        raise ValueError("catalog schema SHA256 differs from sources.lock.yaml")
    if json.loads(schema_data) != CATALOG_SCHEMA:
        raise ValueError("catalog schema differs from the importer contract")
    if sha256_bytes(document_data) != lock["document"]["sha256"]:
        raise ValueError("documentation SHA256 differs from sources.lock.yaml")
    if sha256_bytes(metadata_data) != lock["generated_metadata"]["sha256"]:
        raise ValueError("generated token metadata SHA256 differs from sources.lock.yaml")
    expected_metadata = render_token_metadata(catalog)
    if metadata_data != expected_metadata:
        raise ValueError("generated token metadata differs from the catalog")
    if lock["generated_metadata"]["entries"] != len(catalog["entries"]):
        raise ValueError("generated token metadata entry count differs from the catalog")
    entries = catalog["entries"]
    required_catalog_keys = set(CATALOG_SCHEMA["required"])
    if not required_catalog_keys.issubset(catalog):
        raise ValueError("catalog does not satisfy required schema keys")
    identities = [item["identity"] for item in entries]
    if identities != sorted(identities) or len(identities) != len(set(identities)):
        raise ValueError("catalog identities are unsorted or duplicated")
    allowed = {"runtime", "metadata", "web-only", "deprecated", "component-not-yet-implemented", "unsupported"}
    unclassified = [item["identity"] for item in entries if item["support"] not in allowed]
    if unclassified:
        raise ValueError("unclassified catalog entries: " + ", ".join(unclassified))
    locked_source_paths = {item["path"] for item in lock["sources"]}
    catalog_source_paths = {item["source"]["path"] for item in entries}
    if "LICENSE" not in locked_source_paths or not catalog_source_paths.issubset(locked_source_paths):
        raise ValueError("catalog source locations are not covered by the source lock")
    coverage = catalog["coverage"]
    if coverage["entries"] != len(entries) or any(coverage[key] != 0 for key in ("missing", "duplicates", "unclassified")):
        raise ValueError("catalog coverage summary is inconsistent")
    owners = catalog["component_owners"]
    if owners != sorted(set(owners)) or coverage["component_owners"] != len(owners):
        raise ValueError("component owner inventory is inconsistent")
    marker = f"Catalog SHA256：`{sha256_bytes(catalog_data)}`"
    if marker not in document_data.decode("utf-8"):
        raise ValueError("documentation does not identify the current catalog")


def verify_source(source_root: Path, repo_root: Path) -> None:
    expected = build_outputs(source_root)
    for path, data in zip(output_paths(repo_root), expected):
        if path.read_bytes() != data:
            raise ValueError(f"generated output is stale: {path}")


def self_test(repo_root: Path) -> None:
    verify_repository(repo_root)
    lock_path, catalog_path, schema_path, document_path, metadata_path = output_paths(repo_root)
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    duplicate_catalog = dict(catalog)
    duplicate_catalog["entries"] = list(catalog["entries"]) + [dict(catalog["entries"][0])]
    try:
        render_token_metadata(duplicate_catalog)
    except ValueError:
        pass
    else:
        raise AssertionError("duplicate stable token mapping was accepted")

    missing_kind_catalog = dict(catalog)
    missing_kind_catalog["entries"] = [dict(item) for item in catalog["entries"]]
    del missing_kind_catalog["entries"][0]["value_kind"]
    try:
        render_token_metadata(missing_kind_catalog)
    except KeyError:
        pass
    else:
        raise AssertionError("token metadata with a missing value kind was accepted")

    with tempfile.TemporaryDirectory(prefix="rynui-token-contract-") as directory:
        root = Path(directory)
        targets = output_paths(root)
        for source, target in zip(
            (lock_path, catalog_path, schema_path, document_path, metadata_path), targets
        ):
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
        tampered_catalog = json.loads(targets[1].read_text(encoding="utf-8"))
        tampered_catalog["entries"][0]["upstream_name"] = "tampered"
        targets[1].write_bytes(json_bytes(tampered_catalog))
        try:
            verify_repository(root)
        except ValueError:
            pass
        else:
            raise AssertionError("tampered catalog was accepted")


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--verify-source", action="store_true")
    parser.add_argument("--verify-repository", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.write:
            if args.source_root is None:
                parser.error("--write requires --source-root")
            write_outputs(args.repo_root, build_outputs(args.source_root.resolve()))
        if args.verify_source:
            if args.source_root is None:
                parser.error("--verify-source requires --source-root")
            verify_source(args.source_root.resolve(), args.repo_root.resolve())
        if args.verify_repository:
            verify_repository(args.repo_root.resolve())
        if args.self_test:
            self_test(args.repo_root.resolve())
        if not any((args.write, args.verify_source, args.verify_repository, args.self_test)):
            parser.error("select an operation")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"design-token catalog error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
