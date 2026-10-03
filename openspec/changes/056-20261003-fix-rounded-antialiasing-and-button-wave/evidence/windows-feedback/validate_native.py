"""Replay or verify native Switch timing and RadioButton border-box readback."""
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
DEST = Path(__file__).resolve().parent
MODES = ("system", "1", "1.25", "1.5", "2")
PHASES = ("base", "press-0", "press-50", "held-200", "release-0", "release-50", "settled-200")
Q = 0.12916193  # CSS cubic-bezier(0.42,0,0.58,1) at t=0.25.


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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


def verify(runs):
    if len(runs) != 10 or {(r["configuration"], r["mode"]) for r in runs} != {
            (config, mode) for config in ("Debug", "Release") for mode in MODES}:
        raise RuntimeError("Incomplete native preset/DPI matrix")
    for run in runs:
        if digest(ROOT / run["executable"]) != run["executable_sha256"]:
            raise RuntimeError("Native executable changed after capture")
        directory = DEST / run["id"]
        for item in run["files"]:
            if digest(directory / item["path"]) != item["sha256"]:
                raise RuntimeError("Captured file identity changed: " + item["path"])
        if pixel_checks(directory, run["scale"]) != run["checks"]:
            raise RuntimeError("Saved pixel/geometry checks changed")
    print(f"verified runs={len(runs)} handle_frames={sum(r['checks']['handle_frames'] for r in runs)} "
          f"radio_pixels={sum(r['checks']['radio_pixels'] for r in runs)} max_channel_error="
          f"{max(r['checks']['maximum_channel_error'] for r in runs)} tolerance=2")


def main():
    if "--verify-only" in sys.argv:
        verify(json.loads((DEST / "runs.json").read_text(encoding="utf-8")))
        return
    runs = []
    for config in ("Debug", "Release"):
        exe = ROOT / "out/build/windows-msvc/examples" / config / "rynui_token_gallery.exe"
        for mode in MODES:
            name = config.lower() + "-" + mode
            source = ROOT / "out/056-feedback-native" / name
            directory = DEST / name
            source.mkdir(parents=True, exist_ok=True)
            directory.mkdir(parents=True, exist_ok=True)
            scale = None
            for component in ("switch", "radio"):
                args = [str(exe), f"--{component}-acceptance", f"--evidence-dir={source}"]
                if mode != "system":
                    args.append("--acceptance-scale=" + mode)
                result = subprocess.run(args, cwd=exe.parent, capture_output=True, timeout=90)
                log = (result.stdout + result.stderr).decode("utf-8").replace("\r\n", "\n")
                (directory / (component + ".log")).write_text(log, encoding="utf-8", newline="\n")
                if result.returncode or f"{component}_acceptance=passed gpu_driver=direct3d12 shader_format=DXIL" not in log:
                    raise RuntimeError(log)
                for expected in ("deadline=none disposed=1 exit_code=0", "continuous_releases=8" if component == "switch" else "solid_samples=8"):
                    if expected not in log:
                        raise RuntimeError("Missing contract: " + expected)
                measured = float(re.search(r"\brender_scale=([\d.]+)", log).group(1))
                system = float(re.search(r"\bsystem_display_scale=([\d.]+)", log).group(1))
                close(measured, system if mode == "system" else float(mode), .001)
                if scale is not None:
                    close(measured, scale, .001)
                scale = measured
            save_crops(source, directory, scale)
            checks = pixel_checks(directory, scale)
            files = [{"path": p.name, "sha256": digest(p)} for p in sorted(directory.iterdir()) if p.is_file()]
            runs.append({"id": name, "configuration": config, "mode": mode, "scale": scale,
                         "executable": exe.relative_to(ROOT).as_posix(), "executable_sha256": digest(exe),
                         "checks": checks, "files": files})
            print(name, checks, flush=True)
    (DEST / "runs.json").write_text(json.dumps(runs, indent=2) + "\n", encoding="utf-8", newline="\n")
    verify(runs)


if __name__ == "__main__":
    main()
