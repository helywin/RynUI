#!/usr/bin/env python3
"""Check native GPU readbacks against rounded SDF and control geometry references.

Consumes raw acceptance outputs in ignored out/; never requires Windows-specific
logs or writes verification artifacts into the repository's evidence directories.
"""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import tempfile

from PIL import Image, ImageChops

TOLERANCE = 2
PHASES = ("base", "press-0", "press-50", "held-200", "release-0", "release-50", "settled-200")
Q = 0.12916193  # CSS ease-in-out progress at one quarter of the transition.


def verify_rounded(directory):
    checks = []
    for theme in ("light", "dark"):
        stem = "coverage-" + theme
        pixels = (directory / (stem + ".rgb")).read_bytes()
        with (directory / (stem + ".csv")).open(newline="", encoding="utf-8") as stream:
            regions = list(csv.DictReader(stream))
        if len(regions) != 22 or {row["name"] for row in regions if row["name"].startswith("corners-")} != {
                "corners-" + str(mask) for mask in range(16)}:
            raise RuntimeError("All sixteen corner combinations must be checked")
        with Image.open(directory / (stem + ".bmp")) as image:
            image = image.convert("RGB")
            for region in regions:
                x, y, width, height, offset, partial = (int(region[key]) for key in
                    ("x", "y", "width", "height", "offset", "partial"))
                reference = Image.frombytes("RGB", (width, height), pixels[offset:offset + width * height * 3])
                actual = image.crop((x, y, x + width, y + height))
                difference = ImageChops.difference(actual, reference)
                maximum = max(high for low, high in difference.getextrema())
                if maximum > TOLERANCE:
                    raise RuntimeError(f"{directory.name}/{stem}/{region['name']}: GPU error {maximum} > {TOLERANCE}; "
                                       f"mismatch bounds={difference.getbbox()}")
                if partial < 4:
                    raise RuntimeError("No fractional boundary coverage in " + region["name"])
                checks.append({"capture": stem, "case": region["name"], "pixels": width * height,
                               "maximum_channel_error": maximum, "fractional_reference_pixels": partial})
    return checks


def rows(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def box(row):
    return [float(row[key]) for key in ("x", "y", "width", "height")]


def close(actual, expected, tolerance=0.02):
    if abs(actual - expected) > tolerance:
        raise RuntimeError(f"geometry {actual} != {expected}")


def geometry_checks(directory):
    samples = rows(directory / "handle-geometry.csv")
    if len(samples) != 56 or len({row["name"] for row in samples}) != 56:
        raise RuntimeError("Incomplete native handle timeline")
    groups = {}
    for row in samples:
        groups.setdefault((row["size"], row["rtl"], row["cycle"]), {})[row["phase"]] = row
    if set(groups) != {(size, rtl, cycle) for size in ("middle", "small") for rtl in ("0", "1") for cycle in ("0", "1")}:
        raise RuntimeError("Incomplete direction/size/reverse matrix")
    for (size, rtl, cycle), phases in groups.items():
        if set(phases) != set(PHASES):
            raise RuntimeError("Missing timeline phase")
        base, pressed, partial, held, released, moving, settled = (box(phases[p]) for p in PHASES)
        if base != pressed or held != released:
            raise RuntimeError("Handle jumped at press or release")
        close(held[2], base[2] * 1.3)
        close(partial[2], base[2] * (1 + .3 * Q))
        close(moving[2], base[2] * (1 + .3 * (1 - Q)))
        close(settled[2], base[2])
        at_right = (phases["base"]["checked"] == "1") != (rtl == "1")
        close(held[0] + (held[2] if at_right else 0), base[0] + (base[2] if at_right else 0))
        close(moving[0], base[0] * (1 - Q) + settled[0] * Q - (base[2] * .3 * (1 - Q) if at_right else 0))
        if phases["settled-200"]["checked"] == phases["base"]["checked"]:
            raise RuntimeError("Native input did not toggle the switch")
        for phase, offset in (("press-0", 0), ("press-50", 50000), ("held-200", 200000),
                              ("release-0", 200000), ("release-50", 250000), ("settled-200", 400000)):
            if int(phases[phase]["time_us"]) - int(phases["base"]["time_us"]) != offset:
                raise RuntimeError("Timeline timestamp changed")
    return samples


def crop_box(rect, scale, pad=6):
    x, y, width, height = rect
    return [math.floor((x - pad) * scale), math.floor((y - pad) * scale),
            math.ceil((x + width + pad) * scale), math.ceil((y + height + pad) * scale)]


def save_crops(source, directory, scale):
    samples = geometry_checks(source)
    # Each transition uses one fixed crop containing both anchor positions.
    for row in samples:
        group = [r for r in samples if (r["size"], r["rtl"], r["cycle"]) == (row["size"], row["rtl"], row["cycle"])]
        left = min(box(r)[0] for r in group)
        right = max(box(r)[0] + box(r)[2] for r in group)
        rect = [left, float(row["y"]), right - left, float(row["height"])]
        row["crop_left"], row["crop_top"], row["crop_right"], row["crop_bottom"] = crop_box(rect, scale)
    fills = rows(source / "solid-fill.csv")
    for row in fills:
        row["crop_left"], row["crop_top"], row["crop_right"], row["crop_bottom"] = crop_box(box(row), scale, 2)
    for name, data in (("handle-geometry.csv", samples), ("solid-fill.csv", fills)):
        with (directory / name).open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, list(data[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(data)
        for row in data:
            with Image.open(source / (row["name"] + ".bmp")) as image:
                image.convert("RGB").crop([int(row[k]) for k in ("crop_left", "crop_top", "crop_right", "crop_bottom")]).save(
                    directory / (row["name"] + ".png"))
    for name in ("initial", "solid-large-block"):
        with Image.open(source / (name + ".bmp")) as image:
            image.convert("RGB").save(directory / (name + ".png"))


def pixel_checks(directory, scale):
    handles = geometry_checks(directory)
    for row in handles:
        x, y, width, height = box(row)
        ox, oy = int(row["crop_left"]), int(row["crop_top"])
        with Image.open(directory / (row["name"] + ".png")) as image:
            pixels = image.convert("RGB")
            scan_y = math.floor((y + height / 2) * scale) - oy
            white = [px for px in range(math.floor(x * scale) - 1, math.ceil((x + width) * scale) + 1)
                     if 0 <= px - ox < pixels.width and min(pixels.getpixel((px - ox, scan_y))) >= 245]
            if not white:
                raise RuntimeError("Native GPU handle is missing")
            close(min(white), x * scale, 2)
            close(max(white) + 1, (x + width) * scale, 2)
    fills = rows(directory / "solid-fill.csv")
    if len(fills) != 8 or len({r["name"] for r in fills}) != 8:
        raise RuntimeError("Incomplete light/dark/direction/orientation fill matrix")
    count, maximum, fractional = 0, 0, 0
    for row in fills:
        x, y, width, height = box(row)
        radius = float(row["radius"])
        fg = [float(row[k]) * 255 for k in ("red", "green", "blue")]
        bg = [float(row[k]) * 255 for k in ("background_red", "background_green", "background_blue")]
        ox, oy = int(row["crop_left"]), int(row["crop_top"])
        with Image.open(directory / (row["name"] + ".png")) as image:
            image = image.convert("RGB")
            case_pixels, case_fractional = 0, 0
            for iy in range(image.height):
                py = (iy + oy + .5) / scale
                for ix in range(image.width):
                    px = (ix + ox + .5) / scale
                    shared = (row["vertical"] == "1" and py > y + height - 2) or (row["vertical"] == "0" and
                             (px < x + 2 if row["rtl"] == "1" else px > x + width - 2))
                    rounded = (py < y + height / 2) if row["vertical"] == "1" else (
                        px > x + width / 2 if row["rtl"] == "1" else px < x + width / 2)
                    r = radius if rounded else 0
                    dx, dy = abs(px - x - width / 2) - width / 2 + r, abs(py - y - height / 2) - height / 2 + r
                    d = math.hypot(max(dx, 0), max(dy, 0)) + min(max(dx, dy), 0) - r
                    # Include every fully covered interior pixel of the shared border. Outside it belongs
                    # to a differently colored neighbor, rather than the window background used below.
                    if shared and d > -.5 / scale:
                        continue
                    if d < -3 or d > 1 / scale:
                        continue
                    t = max(0, min(1, d * scale + .5))
                    alpha = 1 - t * t * (3 - 2 * t)
                    expected = [round(f * alpha + b * (1 - alpha)) for f, b in zip(fg, bg)]
                    error = max(abs(a - b) for a, b in zip(image.getpixel((ix, iy)), expected))
                    if error > 2:
                        raise RuntimeError(f"{directory.name}/{row['name']} ({px},{py}): fill error {error} > 2")
                    maximum = max(maximum, error)
                    count += 1
                    case_pixels += 1
                    if .01 < alpha < .99:
                        fractional += 1
                        case_fractional += 1
            if case_pixels < 100 or case_fractional < 4:
                raise RuntimeError("Fill check missed interior or curved AA")
    return {"handle_frames": len(handles), "continuous_releases": 8, "radio_cases": len(fills),
            "radio_pixels": count, "fractional_pixels": fractional, "maximum_channel_error": maximum}


def render_scale(directory):
    log = (directory / "run.log").read_text(encoding="utf-8")
    match = re.search(r"\brender_scale=([\d.]+)", log)
    if match is None or "acceptance=passed" not in log:
        raise RuntimeError("Missing passed native run: " + str(directory))
    return float(match.group(1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="Raw native matrix root")
    parser.add_argument("--work-dir", required=True, type=Path, help="Ignored out/ directory for generated checks")
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    results = []
    for configuration in ("Debug", "Release"):
        for scale in ("1", "1.25", "1.5", "2", "system"):
            directory = args.root / configuration / ("scale-" + scale)
            rounded = directory / "rounded"
            render_scale(rounded)
            with Image.open(rounded / "resized.bmp") as image:
                # SDL window coordinates can map to more pixels on a real HiDPI output.
                if not math.isclose(image.width / image.height, 1420 / 1000, abs_tol=.002):
                    raise RuntimeError("Rounded window did not actually resize: " + str(image.size))
            regions = verify_rounded(rounded)
            switch = directory / "switch"
            radio = directory / "radio"
            density = render_scale(switch)
            if not math.isclose(density, render_scale(radio), abs_tol=.001):
                raise RuntimeError("Switch and Radio scales differ")
            with tempfile.TemporaryDirectory(dir=args.work_dir) as temporary:
                source = Path(temporary) / "source"
                captures = Path(temporary) / "checks"
                source.mkdir()
                captures.mkdir()
                for component in (switch, radio):
                    for path in component.iterdir():
                        target = source / path.name
                        if path.suffix in (".bmp", ".csv") and not target.exists():
                            os.link(path, target)
                save_crops(source, captures, density)
                controls = pixel_checks(captures, density)
            result = {"configuration": configuration, "scale": scale, "rounded_regions": len(regions),
                      "rounded_pixels": sum(r["pixels"] for r in regions),
                      "rounded_max_error": max(r["maximum_channel_error"] for r in regions), **controls}
            results.append(result)
    (args.work_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print("verified", json.dumps({"runs": len(results),
          "rounded_regions": sum(r["rounded_regions"] for r in results),
          "rounded_pixels": sum(r["rounded_pixels"] for r in results),
          "handle_frames": sum(r["handle_frames"] for r in results),
          "radio_cases": sum(r["radio_cases"] for r in results),
          "radio_pixels": sum(r["radio_pixels"] for r in results),
          "maximum_channel_error": max(max(r["rounded_max_error"], r["maximum_channel_error"]) for r in results)}))


if __name__ == "__main__":
    main()
