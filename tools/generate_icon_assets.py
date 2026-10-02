"""Reproducible CFF layers and typed names from all locked Ant SVGs.

Maintenance only: fonttools==4.60.1 in out/icon-tools; normal builds use
committed artifacts. Run with PYTHONPATH=out/icon-tools.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import fontTools
from fontTools.cffLib import PrivateDict
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.areaPen import AreaPen
from fontTools.pens.pointInsidePen import PointInsidePen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.svgLib.path import SVGPath

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "third_party/ant-design-icons"
OUTPUT = ROOT / "src/icons/ant_design_icon_font.inc"
CATALOG = ROOT / "src/icons/ant_design_icon_catalog.inc"
NAMES = ROOT / "include/ryn/generated/icon_names.inc"
MANIFEST = ASSETS / "manifest.json"
LEGACY = [
    ("EyeOutlined", "outlined-eye.svg"),
    ("EyeInvisibleOutlined", "outlined-eye-invisible.svg"),
    ("SearchOutlined", "outlined-search.svg"),
    ("CloseCircleFilled", "filled-close-circle.svg"),
    ("MenuOutlined", "outlined-menu.svg"),
    ("SunOutlined", "outlined-sun.svg"),
    ("MoonOutlined", "outlined-moon.svg"),
    ("UserOutlined", "outlined-user.svg"),
    ("LockOutlined", "outlined-lock.svg"),
    ("CopyOutlined", "outlined-copy.svg"),
    ("CheckOutlined", "outlined-check.svg"),
    ("EditOutlined", "outlined-edit.svg"),
    ("DownOutlined", "outlined-down.svg"),
    ("UpOutlined", "outlined-up.svg"),
]
PRIMITIVES = [
    ("TooltipArrowDown", 0xF000, [(0, 0), (1024, 0), (512, 512)]),
    ("TooltipArrowUp", 0xF001, [(0, 512), (512, 0), (1024, 512)]),
    ("TooltipArrowRight", 0xF002, [(0, 0), (512, 512), (0, 1024)]),
    ("TooltipArrowLeft", 0xF003, [(512, 0), (512, 1024), (0, 512)]),
]


def inventory():
    entries = dict(LEGACY)
    for path in sorted(ASSETS.glob("*.svg")):
        kind, stem = path.stem.split("-", 1)
        suffix = {"outlined": "Outlined", "filled": "Filled", "twotone": "TwoTone"}[kind]
        name = "".join(word[:1].upper() + word[1:] for word in stem.split("-")) + suffix
        if name[:1].isdigit():
            name = "Icon" + name
        if name in entries and entries[name] != path.name:
            raise ValueError("Duplicate icon name: " + name)
        entries[name] = path.name
    legacy_names = {name for name, _ in LEGACY}
    result = LEGACY + sorted((name, file) for name, file in entries.items() if name not in legacy_names)
    if len(result) != 848:
        raise ValueError("Locked catalog must contain 848 icons")
    return result


def contours(recording):
    result = []
    current = None
    for operation, arguments in recording.value:
        if operation == "moveTo":
            current = RecordingPen()
            result.append(current)
        if current is None:
            raise ValueError("Path has no initial move")
        getattr(current, "closePath" if operation == "endPath" else operation)(*arguments)
    return result


def draw_path(element, transform, even_odd, pen):
    recording = RecordingPen()
    wrapper = ET.Element("svg")
    wrapper.append(ET.Element("path", {"d": element.attrib["d"]}))
    SVGPath.fromstring(ET.tostring(wrapper), transform=transform).draw(recording)
    if not even_odd:
        recording.replay(pen)
        return
    # Convert per-path SVG even-odd nesting to CFF nonzero winding.
    values = contours(recording)
    for index, contour in enumerate(values):
        point = contour.value[0][1][0]
        depth = 0
        for other_index, other in enumerate(values):
            if index != other_index:
                inside = PointInsidePen(None, point, evenOdd=True)
                other.replay(inside)
                depth += int(inside.getResult())
        area = AreaPen(None)
        contour.replay(area)
        if area.value and (area.value > 0) != (depth % 2 == 0):
            contour.replay(ReverseContourPen(pen))
        else:
            contour.replay(pen)


def layers(root):
    result = []

    def visit(element, inherited_fill="currentColor", opacity=1.0, even_odd=False):
        fill = element.get("fill", inherited_fill)
        opacity *= float(element.get("fill-opacity", "1"))
        even_odd = element.get("fill-rule", "evenodd" if even_odd else "nonzero") == "evenodd"
        if element.tag == "path":
            if fill not in ("currentColor", "#333", "#E6E6E6"):
                raise ValueError("Unrecognized locked path color: " + fill)
            role = "secondary" if fill == "#E6E6E6" else "primary"
            if not result or result[-1][0:2] != (role, opacity):
                result.append((role, opacity, []))
            result[-1][2].append((element, even_odd))
        for child in element:
            visit(child, fill, opacity, even_odd)

    visit(root)
    return result


def generate():
    if fontTools.version != "4.60.1":
        raise RuntimeError("Regeneration requires fonttools==4.60.1")
    icons = inventory()
    glyph_names = [".notdef"]
    chars = {".notdef": T2CharStringPen(1024, None).getCharString()}
    entries = []
    glyph_map = {}
    for index, (name, filename) in enumerate(icons):
        source = (ASSETS / filename).read_bytes()
        root = ET.fromstring(source)
        x, y, width, height = map(float, root.attrib["viewBox"].split())
        if width != height or width <= 0:
            raise ValueError("Locked icon viewBox must be a nonempty square")
        scale = 1024 / width
        transform = (scale, 0, 0, -scale, -x * scale, 896 + y * scale)
        layer_entries = []
        for layer_index, (role, opacity, paths) in enumerate(layers(root)):
            glyph_name = name if layer_index == 0 else name + "Layer" + str(layer_index)
            codepoint = 0xE000 + index if layer_index == 0 else 0xF0000 + index * 8 + layer_index
            pen = T2CharStringPen(1024, None, roundTolerance=0.01)
            for path, even_odd in paths:
                draw_path(path, transform, even_odd, pen)
            chars[glyph_name] = pen.getCharString()
            chars[glyph_name].private = PrivateDict()
            bounds = chars[glyph_name].calcBounds(None)
            if bounds is None:
                raise ValueError("Empty outline: " + glyph_name)
            glyph_names.append(glyph_name)
            glyph_map[codepoint] = glyph_name
            layer_entries.append({"codepoint": codepoint, "role": role, "opacity": opacity,
                                  "bounds": list(bounds)})
        entries.append({"name": name, "file": filename, "codepoint": 0xE000 + index,
                        "sha256": hashlib.sha256(source).hexdigest(), "layers": layer_entries})
    primitives = []
    for name, codepoint, points in PRIMITIVES:
        pen = T2CharStringPen(1024, None, roundTolerance=0.01)
        pen.moveTo((points[0][0], 896 - points[0][1]))
        for x, y in points[1:]:
            pen.lineTo((x, 896 - y))
        pen.closePath()
        chars[name] = pen.getCharString()
        chars[name].private = PrivateDict()
        glyph_names.append(name)
        glyph_map[codepoint] = name
        primitives.append({"name": name, "codepoint": codepoint, "points": points,
                           "origin": "RynUI", "bounds": list(chars[name].calcBounds(None))})
    builder = FontBuilder(1024, isTTF=False)
    builder.setupGlyphOrder(glyph_names)
    builder.setupCharacterMap(glyph_map)
    builder.setupCFF("RynUIAntIcons", {"FullName": "RynUI Ant Icons", "FamilyName": "RynUI Ant Icons",
                                     "Weight": "Regular"}, chars, {})
    builder.setupHorizontalMetrics({name: (1024, 0) for name in glyph_names})
    builder.setupHorizontalHeader(ascent=896, descent=-128, lineGap=0)
    builder.setupNameTable({"familyName": "RynUI Ant Icons", "styleName": "Regular",
                           "uniqueFontIdentifier": "RynUIAntIcons-4.6.0-2", "fullName": "RynUI Ant Icons",
                           "psName": "RynUIAntIcons", "version": "Version 2.0"})
    builder.setupOS2(sTypoAscender=896, sTypoDescender=-128, sTypoLineGap=0,
                     usWinAscent=896, usWinDescent=128, fsType=0)
    builder.setupPost()
    builder.font.recalcTimestamp = False
    builder.font["head"].created = builder.font["head"].modified = 3800000000
    data = io.BytesIO()
    builder.save(data)
    raw = data.getvalue()
    lines = ["// Generated by tools/generate_icon_assets.py; do not edit.",
             "// Ant Design icons-svg 4.6.0, MIT; see third_party/ant-design-icons/LICENSE.",
             "static constexpr unsigned char ant_design_icon_font_bytes[] = {"]
    lines += ["    " + ", ".join(f"0x{value:02x}" for value in raw[i:i + 16]) + ","
              for i in range(0, len(raw), 16)]
    lines += ["};", ""]
    typed = ["// Generated by tools/generate_icon_assets.py; do not edit."]
    typed += [f"    {name} = {index}," for index, (name, _) in enumerate(icons)]
    typed.append("")
    catalog = ["// Generated by tools/generate_icon_assets.py; do not edit.",
               "static constexpr BundledIconLayer bundled_icon_layers[] = {"]
    offset = 0
    for entry in entries:
        for layer in entry["layers"]:
            catalog.append(f"    {{0x{layer['codepoint']:x}, {str(layer['role'] == 'secondary').lower()}, "
                           f"{layer['opacity']:.6f}F}},")
    catalog += ["};", "static constexpr BundledIconEntry bundled_icon_entries[] = {"]
    for entry in entries:
        catalog.append(f"    {{{offset}, {len(entry['layers'])}, \"{entry['name']}\"}},")
        offset += len(entry["layers"])
    catalog += ["};", ""]
    manifest = {"package": "@ant-design/icons-svg", "version": "4.6.0",
                "upstream_commit": "7f2516ac91226d2b41f93b35cb5197c8d94f7189",
                "source_archive_sha256": "7e07fdcf459f1f2ae6721ca1796811d9fb7737693af0724b55840f4edccde80b",
                "license_sha256": hashlib.sha256((ASSETS / "LICENSE").read_bytes()).hexdigest(),
                "fonttools_version": fontTools.version, "max_layers": max(len(e["layers"]) for e in entries),
                "outline_container_sha256": hashlib.sha256(raw).hexdigest(), "icons": entries,
                "primitives": primitives}
    return {OUTPUT: "\n".join(lines), NAMES: "\n".join(typed), CATALOG: "\n".join(catalog),
            MANIFEST: json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, source in generate().items():
        if args.check:
            if path.read_text(encoding="utf-8") != source:
                raise SystemExit("Generated icon asset out of date: " + str(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source, encoding="utf-8", newline="\n")
    print("848 icons and all color layers reproduce exactly" if args.check else "Generated 848 icons and layers")


if __name__ == "__main__":
    main()
