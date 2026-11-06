"""Freeze Ant Design's MIT Result artwork as PNG bytes for the shared native ImageSource path.

Generator only: CairoSVG and Cairo are tooling dependencies, never RynUI runtime dependencies.
The SVG source copies and intermediate PNGs belong in out, not in the source tree.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

COMMIT = "4a39f54842eade4e565ab336ef6097cd7e723cdd"
SOURCES = {403: "unauthorized.tsx", 404: "noFound.tsx", 500: "serverError.tsx"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("--cairo-bin", type=Path)
    parser.add_argument("--output", type=Path, default=Path("src/icons/result_illustrations.inc"))
    parser.add_argument("--manifest", type=Path, default=Path("third_party/ant-design-result/manifest.json"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    actual = subprocess.check_output(["git", "-C", str(args.upstream), "rev-parse", "HEAD"], text=True).strip()
    if actual != COMMIT:
        raise ValueError(f"Expected {COMMIT}, found {actual}")
    dll = None
    if args.cairo_bin:
        dll = os.add_dll_directory(str(args.cairo_bin))
        os.environ["PATH"] = str(args.cairo_bin) + os.pathsep + os.environ["PATH"]
    import cairosvg
    output = ["// Generated from pinned Ant Design 6.6.5 Result artwork (MIT).",
              f"// Commit {COMMIT}; see third_party/ant-design-result/manifest.json and LICENSE."]
    manifest = {"version": "6.6.5", "commit": COMMIT, "scale": 2, "generator": "CairoSVG " + cairosvg.__version__, "assets": []}
    for status, name in SOURCES.items():
        source = subprocess.check_output(["git", "-C", str(args.upstream), "show", f"{COMMIT}:components/result/{name}"])
        svg = re.search(r"<svg\b.*?</svg>", source.decode("utf-8"), re.S).group()
        svg = svg.replace("<svg ", '<svg xmlns="http://www.w3.org/2000/svg" ', 1)
        for before, after in {"fillRule": "fill-rule", "clipRule": "clip-rule", "fillOpacity": "fill-opacity",
                              "strokeWidth": "stroke-width", "strokeLinecap": "stroke-linecap",
                              "strokeLinejoin": "stroke-linejoin", "strokeOpacity": "stroke-opacity"}.items():
            svg = svg.replace(before, after)
        png = cairosvg.svg2png(bytestring=svg.encode("utf-8"), scale=2)
        output.append(f"constexpr unsigned char result_png_{status}[]{{")
        for start in range(0, len(png), 24):
            output.append("    " + ", ".join(f"0x{x:02x}" for x in png[start:start + 24]) + ",")
        output.append("};")
        manifest["assets"].append({"status": status, "source": f"components/result/{name}",
                                  "sourceSha256": hashlib.sha256(source).hexdigest(),
                                  "pngSha256": hashlib.sha256(png).hexdigest(), "pngBytes": len(png)})
    generated = "\n".join(output) + "\n"
    metadata = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if args.check:
        if args.output.read_text(encoding="utf-8") != generated or args.manifest.read_text(encoding="utf-8") != metadata:
            raise SystemExit("Result artwork differs from pinned generation")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
        args.manifest.write_text(metadata, encoding="utf-8", newline="\n")
        (args.manifest.parent / "LICENSE").write_bytes(subprocess.check_output(["git", "-C", str(args.upstream), "show", f"{COMMIT}:LICENSE"]))
    print("Pinned Result illustrations match" if args.check else "Generated pinned Result illustrations")


if __name__ == "__main__":
    main()
