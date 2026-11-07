#!/usr/bin/env python3
"""Run an automated native test on an isolated, invisible Mutter Wayland display."""

import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log-dir", type=Path, default=Path("out/background-wayland"))
    parser.add_argument("--timeout", type=float, default=30, help="Maximum test duration in seconds")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a test command is required after --")
    root = Path(__file__).resolve().parents[1]
    output_root = root / "out"
    log_dir = args.log_dir.resolve()
    if not log_dir.is_relative_to(output_root):
        parser.error("--log-dir must be inside this repository's ignored out/ directory")
    log_dir.mkdir(parents=True, exist_ok=True)
    # Runtime sockets need a native filesystem and mode 0700; mounted source
    # trees (for example WSL drvfs) may provide neither. Logs stay in out/.
    with tempfile.TemporaryDirectory(prefix="rynui-wayland-") as runtime:
        env = dict(os.environ, XDG_RUNTIME_DIR=runtime, WAYLAND_DISPLAY="rynui-test",
                   SDL_VIDEODRIVER="wayland", SDL_GPU_DRIVER="vulkan",
                   GIO_USE_VFS="local", GSETTINGS_BACKEND="memory")
        for key in ("DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "WAYLAND_SOCKET"):
            env.pop(key, None)
        with (log_dir / "compositor.log").open("w") as log:
            compositor = subprocess.Popen(
                ["dbus-run-session", "--", "mutter", "--headless", "--wayland", "--no-x11",
                 "--virtual-monitor", "1920x1200", "--wayland-display", env["WAYLAND_DISPLAY"]],
                env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                deadline = time.monotonic() + 10
                while not (Path(runtime) / env["WAYLAND_DISPLAY"]).exists():
                    if compositor.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError(f"Background Wayland unavailable; see {log_dir / 'compositor.log'}")
                    time.sleep(.1)
                test = subprocess.Popen(command, env=env, start_new_session=True)
                try:
                    return test.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    print("Background test timed out; no desktop fallback was started.", flush=True)
                    return 124
                finally:
                    if test.poll() is None:
                        os.killpg(test.pid, signal.SIGTERM)
                        try:
                            test.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            os.killpg(test.pid, signal.SIGKILL)
                            test.wait()
            finally:
                if compositor.poll() is None:
                    os.killpg(compositor.pid, signal.SIGTERM)
                    try:
                        compositor.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(compositor.pid, signal.SIGKILL)
                        compositor.wait()


if __name__ == "__main__":
    raise SystemExit(main())
