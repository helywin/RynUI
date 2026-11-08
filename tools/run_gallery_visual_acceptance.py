"""Capture real Gallery samples with a hidden native GPU window; keep outputs in out/."""

import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

from PIL import Image, ImageChops, ImageDraw, ImageStat


ROOT = Path(__file__).resolve().parents[1]


def registered_routes():
    source = (ROOT / "examples/token_gallery/token_gallery_definition.cpp").read_text(encoding="utf-8")
    descriptors = re.findall(
        r'\{\{"(gallery\.sample\.[^"]+)",\s*"(ant\.component\.[^"]+)",\s*'
        r'(?:GalleryInputFamily::(\w+)|\{\})', source)
    routes = {}
    for sample, component, family in descriptors:
        page = component.removeprefix("ant.component.")
        if family and family != "input":
            page += "/" + family.replace("_", "-")
        routes.setdefault(page, []).append(sample)
    if len(routes) < 40:
        raise RuntimeError("Gallery registration discovery is incomplete")
    return routes


def collect(directory):
    manifest = directory / "captures.tsv"
    if not manifest.exists():
        return [], []
    captures = []
    warnings = []
    normal = {}
    for fields in csv.reader(manifest.read_text(encoding="utf-8").splitlines(), delimiter="\t"):
        case, page, sample, state, tile, scale, *rest = fields
        left, top, right, bottom = [int(float(value)) for value in rest[:4]]
        bmp = directory / rest[4]
        with Image.open(bmp) as frame:
            if not (0 <= left < right <= frame.width and 0 <= top < bottom <= frame.height):
                raise RuntimeError(f"Capture rectangle escaped GPU target: {bmp}")
            image = frame.convert("RGB").crop((left, top, right, bottom))
            context = None
            if case in ("0", "3") and tile == "0" and state == "normal":
                context = bmp.with_suffix(".context.png")
                frame.save(context)
        png = bmp.with_suffix(".png")
        image.save(png)
        bmp.unlink()
        record = dict(case=int(case), page=page, sample=sample, state=state, tile=int(tile),
                      scale=float(scale), image=str(png.relative_to(ROOT)), size=list(image.size))
        if context:
            record["context"] = str(context.relative_to(ROOT))
        if max(ImageStat.Stat(image).stddev) < 1:
            warnings.append(dict(kind="blank", **record))
        key = (case, sample)
        if state == "normal" and tile == "0":
            normal[key] = png
        if state == "roundtrip" and key in normal:
            with Image.open(normal[key]) as before:
                if before.size != image.size:
                    warnings.append(dict(kind="roundtrip_crop_size", before=list(before.size), **record))
                else:
                    diff = ImageChops.difference(before, image)
                    mean = sum(ImageStat.Stat(diff).mean) / 3
                    if mean > 0.5:
                        warnings.append(dict(kind="roundtrip_pixels", mean_byte_difference=mean, **record))
        captures.append(record)
    current = directory / "current.bmp"
    if current.exists():
        current.unlink()
    return captures, warnings


def sheets(captures, output):
    # Keep original local PNGs; contact sheets only index the visual review.
    selected = [item for item in captures if item["case"] in (0, 3) and item["state"] != "roundtrip"]
    paths = []
    for start in range(0, len(selected), 6):
        batch = selected[start:start + 6]
        sheet = Image.new("RGB", (1560, ((len(batch) + 2) // 3) * 520), "#e5e5e5")
        draw = ImageDraw.Draw(sheet)
        for index, item in enumerate(batch):
            x = index % 3 * 520
            y = index // 3 * 520
            draw.text((x + 8, y + 8), f'{item["sample"]}\ncase {item["case"]} {item["state"]} tile {item["tile"]}', fill="black")
            with Image.open(ROOT / item["image"]) as image:
                image.thumbnail((504, 472))
                sheet.paste(image, (x + 8, y + 42))
        path = output / f"sheet-{start // 6}.png"
        sheet.save(path)
        paths.append(str(path.relative_to(ROOT)))
    return paths


def drawer_numeric(directory, cases):
    path = directory / "drawer-motion.tsv"
    rows = list(csv.reader(path.read_text(encoding="utf-8").splitlines(), delimiter="\t")) if path.exists() else []
    expected = {(case, edge, phase) for case in range(cases)
                for edge in ("left", "right", "top", "bottom")
                for phase in ("open-t0", "open-half", "open-end", "close-t0", "close-half", "close-end")}
    actual = {(int(row[0]), row[1], row[2]) for row in rows if len(row) == 6 and row[-1] == "pass"}
    summaries = {int(row[0]) for row in rows if len(row) == 5 and row[1] == "nested/reversal/scales/cleanup"
                 and row[-1] == "pass"}
    passed = expected <= actual and summaries == set(range(cases))
    return dict(passed=passed, placement_time_samples=len(expected & actual),
                expected_samples=len(expected), nested_scale_cleanup_cases=sorted(summaries))


def notice_loading_pixels(directory, page, cases):
    stdout = (directory / "stdout.log").read_text(encoding="utf-8")
    rectangles = re.findall(r"notice_loading_icon_rect=(\d+),(\d+),(\d+),(\d+)", stdout)
    if len(rectangles) != cases:
        return dict(passed=False, error="Missing native Icon rectangle for a case")
    results = []
    for case, rectangle in enumerate(rectangles):
        box = tuple(map(int, rectangle))
        crops = []
        for phase in ("loading-first", "loading-second", "info-stopped", "loading-restarted"):
            path = directory / f"ant.component.{page}-loading-{case}-{phase}.bmp"
            if not path.exists():
                return dict(passed=False, error=f"Missing native frame: {case}/{phase}")
            with Image.open(path) as frame:
                if not (0 <= box[0] < box[2] <= frame.width and 0 <= box[1] < box[3] <= frame.height):
                    return dict(passed=False, error="Native Icon rectangle escaped the GPU target")
                crop = frame.convert("RGB").crop(box)
            crop.save(path.with_suffix(".icon.png"))
            crops.append(crop)
        difference = ImageChops.difference(crops[0], crops[1])
        mean = sum(ImageStat.Stat(difference).mean) / 3
        results.append(dict(case=case, passed=difference.getbbox() is not None and mean > .5,
                            physical_rectangle=list(box), rotation_mean_byte_difference=mean))
    return dict(passed=all(case["passed"] for case in results), cases=results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "out/build/windows-msvc/examples/Release/rynui_token_gallery.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "out/gallery-visual-20261106")
    parser.add_argument("--page", action="append")
    parser.add_argument("--cases", type=int, default=4, choices=(1, 3, 4))
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "out"):
        raise ValueError("All visual outputs must stay inside this repository's out/")
    output.mkdir(parents=True, exist_ok=True)
    routes = registered_routes()
    chosen = args.page or list(routes)
    report = dict(source=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  mode="hidden-native-gpu-readback", routes=[], unregistered=[])
    overlay = json.loads((ROOT / "gallery/ant-design/6.6.5/support-overlay.json").read_text(encoding="utf-8"))
    registered = {"ant.component." + page.split("/")[0] for page in routes}
    report["unregistered"] = [{"component": item["identity"], "status": item["status"], "reason": "no directly registered sample"}
                              for item in overlay["entries"] if item["identity"] not in registered]
    for page in chosen:
        if page not in routes:
            raise ValueError(f"No registered samples for {page}")
        directory = output / page.replace("/", "-")
        directory.mkdir(parents=True, exist_ok=True)
        if (directory / "captures.tsv").exists():
            raise ValueError(f"Use a fresh output directory; existing manifest: {directory}")
        command = [str(args.binary.resolve()), "--page-acceptance", "--page-visual", "--page-timeout=120",
                   f"--page-output={directory / 'result.json'}"] + [f"--page={page}"] * args.cases
        error = ""
        with (directory / "stdout.log").open("w", encoding="utf-8") as stdout, \
                (directory / "stderr.log").open("w", encoding="utf-8") as stderr:
            try:
                process = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr, timeout=140, check=False)
                code = process.returncode
            except subprocess.TimeoutExpired:
                code = -1
                error = "external 140s timeout"
        captures, warnings = collect(directory)
        result = json.loads((directory / "result.json").read_text(encoding="utf-8")) if (directory / "result.json").exists() else {}
        record = dict(page=page, exit=code, error=error or result.get("error", ""), expected_samples=routes[page],
                      captures=captures, warnings=warnings, sheets=sheets(captures, directory), native=result,
                      visual_verdict="pending image inspection")
        if page == "drawer":
            record["numeric"] = drawer_numeric(directory, args.cases)
            record["visual_verdict"] = "numeric motion/readback passed" if code == 0 and record["numeric"]["passed"] else "numeric motion/readback failed"
            if not record["numeric"]["passed"]:
                record["exit"] = code or 1
        if page in ("message", "notification"):
            record["loading_pixels"] = notice_loading_pixels(directory, page, args.cases)
            if not record["loading_pixels"]["passed"]:
                record["exit"] = code or 1
        report["routes"].append(record)
        (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f'{page}: exit={code} captures={len(captures)} warnings={len(warnings)} {record["error"]}', flush=True)
    return 1 if any(item["exit"] != 0 for item in report["routes"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
