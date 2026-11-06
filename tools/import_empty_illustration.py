"""Adapt the pinned MIT Empty artwork to native IconVector paths; no runtime SVG dependency."""
import argparse
import hashlib
import re
import subprocess
from pathlib import Path
from xml.etree import ElementTree as ET
from fontTools.pens.recordingPen import RecordingPen
from fontTools.svgLib.path import SVGPath

COMMIT = "4a39f54842eade4e565ab336ef6097cd7e723cdd"
ROLES = ["shadowColor", "borderColor", "panelBgColor", "detailColor", "iconColor"]


def generate(upstream: Path) -> str:
    actual = subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()
    if actual != COMMIT:
        raise ValueError(f"Expected Ant Design 6.6.5 {COMMIT}, found {actual}")
    source = (upstream / "components/empty/empty.tsx").read_text(encoding="utf-8")
    if hashlib.sha256(source.encode("utf-8")).hexdigest() != "401c430b497cb837fe55036e240e9245f6657e07b8cc44ff9ce8c7676ca08eba":
        raise ValueError("Pinned Empty artwork content does not match its source SHA256")
    svg = re.search(r"<svg\b.*?</svg>", source, re.S).group()
    svg = re.sub(r"<title>.*?</title>", "", svg, flags=re.S)
    svg = re.sub(r"fill=\{(\w+)\}", r'fill="\1"', svg)
    svg = svg.replace("fillRule", "fill-rule").replace("fillOpacity", "fill-opacity")
    root = ET.fromstring(svg)
    records = {role: [] for role in ROLES}

    def visit(element, parents=(), fill="none"):
        fill = element.get("fill", fill)
        if element.tag.rsplit("}", 1)[-1] in {"path", "ellipse", "circle"}:
            wrapper = ET.Element("svg")
            wrapper.append(element)
            pen = RecordingPen()
            SVGPath.fromstring(ET.tostring(wrapper)).draw(pen)
            dx = 0.0
            dy = 0.0
            for attrs in parents:
                if "transform" in attrs:
                    match = re.fullmatch(r"translate\(\s*([-\d.]+)[ ,]+([-\d.]+)\s*\)", attrs["transform"])
                    if not match:
                        raise ValueError(f"Unsupported artwork transform: {attrs['transform']}")
                    dx += float(match[1])
                    dy += float(match[2])
            transformed = [(op, tuple((p[0] + dx, p[1] + dy) for p in points)) for op, points in pen.value]
            records[fill].append((element.get("fill-opacity", "1"), transformed))
        else:
            attrs = {key: value for key, value in element.attrib.items() if key in {"transform"}}
            for child in element:
                visit(child, parents + (attrs,), fill)
    visit(root)

    def number(value):
        value = f"{value:.6f}".rstrip("0").rstrip(".")
        return (value if value not in {"", "-0"} else "0") + "F" if "." in value else (value or "0") + ".0F"

    def point(value):
        return "{" + ", ".join(number(v) for v in value) + "}"

    output = ["// Generated native adaptation of Ant Design 6.6.5 Empty artwork (MIT).",
              f"// Source commit {COMMIT}; regenerate with tools/import_empty_illustration.py.",
              "// Copyright (c) 2015-present Ant UED; see third_party/ant-design-empty/LICENSE.",
              "std::array<IconSource, 5> empty_illustration_sources() {", "    return {"]
    for role in ROLES:
        output.append("        IconVector{{0, 0, 184, 152}, {")
        for opacity, commands in records[role]:
            output.append("            IconPath{IconColorRole::Primary, {")
            for operation, points in commands:
                name = {"moveTo": "IconMove", "lineTo": "IconLine", "curveTo": "IconCubic", "qCurveTo": "IconQuadratic", "closePath": "IconClose", "endPath": "IconClose"}[operation]
                output.append("                " + name + "{" + ", ".join(point(p) for p in points) + "},")
            output.append("            }, " + number(float(opacity)) + "},")
        output.append("        }},")
    output.extend(["    };", "}", ""])
    return "\n".join(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "src/icons/empty_illustration.inc")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = generate(args.upstream)
    if args.check:
        if args.output.read_text(encoding="utf-8") != generated:
            raise SystemExit("Empty native artwork differs from the pinned generation")
        print("Empty artwork matches the pinned source and native adaptation.")
    else:
        args.output.write_text(generated, encoding="utf-8", newline="\n")
