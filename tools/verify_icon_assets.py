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
require(len(manifest["icons"]) == 848, "Locked icon inventory changed")
require(len({item["name"] for item in manifest["icons"]}) == 848, "Duplicate icon name")
require(manifest["source_archive_sha256"] ==
        "7e07fdcf459f1f2ae6721ca1796811d9fb7737693af0724b55840f4edccde80b",
        "Source archive changed")
require(manifest["max_layers"] == 4, "Locked maximum layer count changed")
require([item["name"] for item in manifest["icons"][:14]] ==
        ["EyeOutlined", "EyeInvisibleOutlined", "SearchOutlined", "CloseCircleFilled",
         "MenuOutlined", "SunOutlined", "MoonOutlined", "UserOutlined", "LockOutlined",
         "CopyOutlined", "CheckOutlined", "EditOutlined", "DownOutlined", "UpOutlined"],
        "Legacy icon values changed")
counts = {"outlined": 0, "filled": 0, "twotone": 0}
codepoints = set()
require([item["name"] for item in manifest["primitives"]] ==
        ["TooltipArrowDown", "TooltipArrowUp", "TooltipArrowRight", "TooltipArrowLeft"],
        "Private arrow primitives changed")
for index, item in enumerate(manifest["primitives"]):
    require(item["codepoint"] == 0xF000 + index and item["origin"] == "RynUI",
            "Private primitive collided with Ant catalog")
    require(len(item["points"]) == 3 and len(item["bounds"]) == 4,
            "Private primitive geometry is invalid")
for index, item in enumerate(manifest["icons"]):
    require(item["codepoint"] == 0xE000 + index, "Icon codepoint drifted")
    source = (asset_dir / item["file"]).read_bytes()
    require(sha(source) == item["sha256"], f"Icon source changed: {item['name']}")
    svg = ET.fromstring(source)
    require(svg.tag.endswith("svg") and "viewBox" in svg.attrib,
            f"Invalid SVG: {item['name']}")
    require(any(child.tag.endswith("path") for child in svg.iter()),
            f"Missing path: {item['name']}")
    counts[item["file"].split("-", 1)[0]] += 1
    require(1 <= len(item["layers"]) <= 4, "Layer count changed")
    for layer_index, layer in enumerate(item["layers"]):
        expected = 0xE000 + index if layer_index == 0 else 0xF0000 + index * 8 + layer_index
        require(layer["codepoint"] == expected and expected not in codepoints,
                "Layer codepoint changed or collided")
        codepoints.add(expected)
        require(layer["role"] in ("primary", "secondary") and 0 < layer["opacity"] <= 1,
                "Layer material is invalid")
        bounds = layer["bounds"]
        require(len(bounds) == 4 and bounds[2] > bounds[0] and bounds[3] > bounds[1],
                "Empty layer outline")
require(counts == {"outlined": 447, "filled": 251, "twotone": 150}, "Catalog families changed")
typed = (root / "include/ryn/generated/icon_names.inc").read_text(encoding="utf-8")
names = re.findall(r"^\s+(\w+) = (\d+),$", typed, flags=re.M)
require(names == [(item["name"], str(index)) for index, item in enumerate(manifest["icons"])],
        "Typed catalog differs from manifest")

generated = (root / "src/icons/ant_design_icon_font.inc").read_text(encoding="utf-8")
raw = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", generated))
require(raw.startswith(b"OTTO") and sha(raw) == manifest["outline_container_sha256"],
        "Embedded icon outlines are missing or changed")
print("Locked icon sources, license, codepoints and embedded outlines verified")
