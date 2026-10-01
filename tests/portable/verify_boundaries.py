"""Exercise configuration guards and inspect the actual headless build graph."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def configure(project, expected):
    result = subprocess.run(["cmake", "-S", str(project), "-B", str(project / "build"),
                             "-G", "Ninja Multi-Config"], capture_output=True, text=True)
    if result.returncode == 0 or expected not in result.stdout + result.stderr:
        raise AssertionError(f"guard did not reject {expected}: {result.stdout}{result.stderr}")


args = argparse.ArgumentParser()
args.add_argument("--source", type=Path, required=True)
args.add_argument("--build", type=Path, required=True)
options = args.parse_args()
commands = json.loads((options.build / "compile_commands.json").read_text(encoding="utf-8"))
for command in commands:
    text = json.dumps(command).replace("\\\\", "/").lower()
    assert not any(name in text for name in ("sdl3", "shadercross", "default_font_chain", "fontconfig", "libdecor")), text
assert any("component/" in entry["file"].replace("\\", "/") for entry in commands)
for ninja in options.build.rglob("*.ninja"):
    text = ninja.read_text(encoding="utf-8").lower()
    assert not any(name in text for name in ("sdl3", "shadercross", "libdecor", "default_font_chain")), ninja

with tempfile.TemporaryDirectory(prefix="rynui-boundary-") as temporary:
    root = Path(temporary)
    module = (options.source / "cmake").as_posix()
    for index, (settings, expected) in enumerate([
        ('set(RYNUI_PLATFORM_BACKEND HEADLESS)\nset(RYNUI_RENDER_BACKENDS SDL_GPU)', "SDL_GPU requires"),
        ('set(RYNUI_PLATFORM_BACKEND UNKNOWN)', "Unsupported RYNUI_PLATFORM_BACKEND"),
        ('set(RYNUI_RENDER_BACKENDS UNKNOWN)', "Unsupported renderer backend"),
        ('set(RYNUI_RENDER_BACKENDS "")', "must select at least one"),
    ]):
        project = root / str(index)
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\n{settings}\ninclude("{module}/RynUIBackends.cmake")\n', encoding="utf-8")
        configure(project, expected)
    project = root / "includes"
    (project / "src/component").mkdir(parents=True)
    (project / "src/component/leak.hpp").write_text('#include <SDL3/SDL.h>\n', encoding="utf-8")
    (project / "CMakeLists.txt").write_text(
        f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\nrynui_verify_core_includes("${{CMAKE_CURRENT_SOURCE_DIR}}/src")\n', encoding="utf-8")
    configure(project, "Portable Core include violation")
    project = root / "links"
    project.mkdir()
    (project / "CMakeLists.txt").write_text(
        f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\n'
        'add_library(core INTERFACE)\nadd_library(middle INTERFACE)\nadd_library(rynui_platform_sdl INTERFACE)\n'
        'target_link_libraries(core INTERFACE middle)\ntarget_link_libraries(middle INTERFACE rynui_platform_sdl)\n'
        'rynui_assert_portable_link_closure(core)\n', encoding="utf-8")
    configure(project, "forbidden dependency")
print("Actual headless compilation graph and negative boundary fixtures passed")
