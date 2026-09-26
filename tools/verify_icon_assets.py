"""Check committed Ant icon provenance without maintenance dependencies."""

import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
asset_dir = root / "third_party/ant-design-icons"
manifest = json.loads((asset_dir / "manifest.json").read_text(encoding="utf-8"))
lock = (root / "cmake/dependencies/RynUIDependencyLock.cmake").read_text(encoding="utf-8")


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


require(manifest["package"] == "@ant-design/icons-svg", "Icon package drifted")
require(f'RYNUI_ANT_ICONS_VERSION "{manifest["version"]}"' in lock,
        "Icon version is not locked")
require(f'RYNUI_ANT_ICONS_COMMIT "{manifest["upstream_commit"]}"' in lock,
        "Icon upstream commit is not locked")
require(sha((asset_dir / "LICENSE").read_bytes()) == manifest["license_sha256"],
        "Icon license changed")
require(len(manifest["icons"]) == 9, "Initial icon inventory changed")
require(len({item["name"] for item in manifest["icons"]}) == 9, "Duplicate icon name")
for index, item in enumerate(manifest["icons"]):
    require(item["codepoint"] == 0xE000 + index, "Icon codepoint drifted")
    source = (asset_dir / item["file"]).read_bytes()
    require(sha(source) == item["sha256"], f"Icon source changed: {item['name']}")
    svg = ET.fromstring(source)
    require(svg.tag.endswith("svg") and "viewBox" in svg.attrib,
            f"Invalid SVG: {item['name']}")
    require(any(child.tag.endswith("path") for child in svg.iter()),
            f"Missing path: {item['name']}")

generated = (root / "src/icons/ant_design_icon_font.inc").read_text(encoding="utf-8")
raw = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", generated))
require(raw.startswith(b"OTTO") and sha(raw) == manifest["outline_container_sha256"],
        "Embedded icon outlines are missing or changed")
print("Locked icon sources, license, codepoints and embedded outlines verified")
