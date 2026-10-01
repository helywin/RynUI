"""Run real SDL Gallery acceptance and capture two Win32 resize operations."""
import ctypes
import argparse
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import subprocess
import time
from PIL import Image, ImageGrab

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
root = parser.parse_args().root.resolve()
if not (root / "CMakePresets.json").is_file():
    raise RuntimeError("Run from the RynUI workspace or pass --root")
output = root / "openspec/changes/035-20261002-establish-portable-backend-foundation/evidence/windows"
output.mkdir(parents=True, exist_ok=True)
user = ctypes.WinDLL("user32", use_last_error=True)
user.SetProcessDpiAwarenessContext.argtypes = [W.HANDLE]
user.SetProcessDpiAwarenessContext(W.HANDLE(-4))
callback_type = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
user.EnumWindows.argtypes = [callback_type, W.LPARAM]
user.GetWindowThreadProcessId.argtypes = [W.HWND, ctypes.POINTER(W.DWORD)]
user.IsWindowVisible.argtypes = [W.HWND]
user.GetClientRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
user.ClientToScreen.argtypes = [W.HWND, ctypes.POINTER(W.POINT)]
user.SetWindowPos.argtypes = [W.HWND, W.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, W.UINT]
user.SetForegroundWindow.argtypes = [W.HWND]

def find_window(pid):
    found = []
    @callback_type
    def visit(hwnd, unused):
        owner = W.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True
    user.EnumWindows(visit, 0)
    return found[0] if found else None

def client(hwnd):
    rect, point = W.RECT(), W.POINT()
    if not user.GetClientRect(hwnd, ctypes.byref(rect)) or not user.ClientToScreen(hwnd, ctypes.byref(point)):
        raise ctypes.WinError(ctypes.get_last_error())
    return point.x, point.y, point.x + rect.right, point.y + rect.bottom

def hash_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

records = []
for configuration in ("Debug", "Release"):
    executable = root / f"out/build/windows-msvc/examples/{configuration}/rynui_token_gallery.exe"
    with (output / f"{configuration.lower()}-resize.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(executable), "--smoke"], stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 10
        hwnd = None
        while time.monotonic() < deadline and process.poll() is None:
            hwnd = find_window(process.pid)
            if hwnd: break
            time.sleep(0.02)
        if not hwnd: raise RuntimeError("Gallery did not create a native window")
        user.SetForegroundWindow(hwnd)
        time.sleep(0.25)
        sizes = []
        for name, width, height in (("wide", 1200, 840), ("narrow", 900, 700)):
            if not user.SetWindowPos(hwnd, None, 40, 40, width, height, 0x14):
                raise ctypes.WinError(ctypes.get_last_error())
            time.sleep(0.3)
            box = client(hwnd)
            capture = output / f"{configuration.lower()}-resize-{name}.png"
            ImageGrab.grab(bbox=box).save(capture)
            sizes.append({"outer_size": [width, height], "client_size": [box[2]-box[0], box[3]-box[1]],
                          "capture": capture.name, "sha256": hash_file(capture)})
        result = process.wait(timeout=30)
        if result: raise RuntimeError(f"{configuration} Gallery resize exited {result}")
    for name, arguments in (("system", []), ("scale-2", ["--acceptance-scale=2.0"])):
        directory = output / f"{configuration.lower()}-{name}"
        completed = subprocess.run([str(executable), "--typography-acceptance", f"--evidence-dir={directory}", *arguments],
                                   capture_output=True, text=True, encoding="utf-8", timeout=30)
        (output / f"{configuration.lower()}-{name}.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
        if completed.returncode or "typography_acceptance=passed" not in completed.stdout:
            raise RuntimeError(completed.stdout + completed.stderr)
        captures = []
        for bitmap in sorted(directory.glob("*.bmp")):
            png = bitmap.with_suffix(".png")
            with Image.open(bitmap) as pixels: pixels.save(png)
            captures.append({"path": png.relative_to(output).as_posix(), "sha256": hash_file(png)})
            bitmap.unlink()
        records.append({"configuration": configuration, "mode": name, "exit_code": completed.returncode,
                        "executable_sha256": hash_file(executable), "captures": captures, "log": completed.stdout})
    records.append({"configuration": configuration, "mode": "native-resize", "exit_code": result, "sizes": sizes})
(output / "runs.json").write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"Windows Debug/Release resize and system/2.0 render scale passed; {len(records)} runs")
