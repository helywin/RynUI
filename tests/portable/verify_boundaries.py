"""Exercise configuration guards and inspect the actual headless build graph."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def configure(project, expected=None):
    result = subprocess.run(["cmake", "-S", str(project), "-B", str(project / "build"),
                             "-G", "Ninja Multi-Config"], capture_output=True, text=True)
    if expected is None:
        if result.returncode != 0:
            raise AssertionError(f"legal dependency was rejected: {result.stdout}{result.stderr}")
    elif result.returncode == 0 or expected not in result.stdout + result.stderr:
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
effect_packing = [entry for entry in commands
                  if entry["file"].replace("\\", "/").endswith("/rounded_effect_packing.cpp")]
assert effect_packing and all("rynui_renderer_common.dir" in entry["command"] for entry in effect_packing)
for entry in commands:
    if "rynui_graphics.dir" in entry["command"]:
        assert "/renderer/" not in entry["file"].replace("\\", "/"), entry
assert not (options.source / "src/graphics/rounded_effect_gpu.hpp").exists()
assert not (options.source / "src/graphics/rounded_effect_gpu.cpp").exists()
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
    for index, (area, header) in enumerate([
        ("component", "SDL3/SDL.h"), ("input", "windows.h"),
        ("renderer/common", "renderer/future_backend/device.hpp"),
        ("component", "renderer/common/scene_metrics.hpp"),
        ("graphics", "../renderer/common/scene_packing.hpp"),
        ("renderer/common", "renderer/common/../sdl/device.hpp"),
    ]):
        project = root / f"includes-{index}"
        (project / "src" / area).mkdir(parents=True)
        (project / "src" / area / "leak.hpp").write_text(f'#include <{header}>\n', encoding="utf-8")
        (project / "CMakeLists.txt").write_text(
            f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\nrynui_verify_core_includes("${{CMAKE_CURRENT_SOURCE_DIR}}/src")\n', encoding="utf-8")
        configure(project, "Portable Core include violation")
    project = root / "links"
    project.mkdir()
    (project / "CMakeLists.txt").write_text(
        f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\n'
        'add_library(core INTERFACE)\nadd_library(middle INTERFACE)\nadd_library(rynui_platform_sdl INTERFACE)\n'
        'target_link_libraries(core INTERFACE "$<$<CONFIG:Debug>:middle>")\ntarget_link_libraries(middle INTERFACE rynui_platform_sdl)\n'
        'rynui_assert_portable_link_closure(core)\n', encoding="utf-8")
    configure(project, "forbidden dependency")
    for name, links in [
        ("direct", "target_link_libraries(core INTERFACE rynui_renderer_common)"),
        ("conditional-alias", 'target_link_libraries(core INTERFACE "$<LINK_ONLY:$<$<CONFIG:Debug>:middle>>")\n'
         'target_link_libraries(middle INTERFACE common_alias)'),
    ]:
        project = root / f"core-links-{name}"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\n'
            'add_library(core INTERFACE)\nadd_library(middle INTERFACE)\nadd_library(rynui_renderer_common INTERFACE)\n'
            'add_library(common_alias ALIAS rynui_renderer_common)\n'
            f'{links}\nrynui_assert_core_link_closure(core)\n', encoding="utf-8")
        configure(project, "forbidden dependency")
    project = root / "legal-common"
    (project / "src/graphics").mkdir(parents=True)
    (project / "src/renderer/common").mkdir(parents=True)
    (project / "src/graphics/logical.hpp").write_text('#include <runtime/geometry.hpp>\n', encoding="utf-8")
    (project / "src/renderer/common/packing.hpp").write_text(
        '#include "../../graphics/logical.hpp"\n#include "renderer/common/scene_metrics.hpp"\n', encoding="utf-8")
    (project / "CMakeLists.txt").write_text(
        f'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\ninclude("{module}/RynUIArchitecture.cmake")\n'
        'rynui_verify_core_includes("${CMAKE_CURRENT_SOURCE_DIR}/src")\n'
        'add_library(rynui_graphics INTERFACE)\nadd_library(rynui_renderer_common INTERFACE)\n'
        'add_library(rynui_renderer_recording INTERFACE)\n'
        'target_link_libraries(rynui_renderer_common INTERFACE rynui_graphics)\n'
        'target_link_libraries(rynui_renderer_recording INTERFACE rynui_renderer_common)\n'
        'rynui_assert_core_link_closure(rynui_graphics)\n'
        'rynui_assert_portable_link_closure(rynui_renderer_common)\n'
        'rynui_assert_portable_link_closure(rynui_renderer_recording)\n', encoding="utf-8")
    configure(project)
print("Actual headless/Core build graph, negative renderer dependency guards and legal common dependencies passed")
