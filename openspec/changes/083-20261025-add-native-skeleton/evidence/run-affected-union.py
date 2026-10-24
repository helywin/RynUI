"""Run the change-scoped, deduplicated Windows MSVC HEADLESS regression union."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser()
parser.add_argument("configuration", choices=("Debug", "Release"))
args = parser.parse_args()
preset = f"windows-msvc-headless-{args.configuration.lower()}"
portable = set("""
default_theme theme_algorithm theme_runtime theme_scope_component
input_component input_variant input_actions input_caret_blink input_public_api
button_component select_model select_theme select_public_api select_component
modal_model modal_public_api modal_theme modal_boundary modal_component
focus_order focus_state focus_lifecycle interaction_registry pointer_route
text_input_session text_editor text_history text_reconcile
scene_backend logical_scene_packing quad_primitive rounded_effect_scene
backend_boundaries
""".split())
ordinary = set("""
feedback_public_api feedback_theme feedback_component
collapse_public_api collapse_theme collapse_component time_navigation_merge
""".split())
raw = subprocess.check_output(["ctest", "--preset", preset, "--show-only=json-v1"], cwd=ROOT)
tests = json.loads(raw)["tests"]
selected = sorted({test["name"] for test in tests if (
    test["name"].startswith("rynui.skeleton_") or
    test["name"].removeprefix("rynui.portable.") in portable or
    test["name"].removeprefix("rynui.") in ordinary
)})
assert selected and all(f"rynui.portable.{name}" in selected for name in portable)
assert all(f"rynui.{name}" in selected for name in ordinary)
targets = sorted({
    "rynui_skeleton_integration_tests" if name.startswith("rynui.skeleton_integration_") else
    "rynui_portable_" + name.removeprefix("rynui.portable.") if name.startswith("rynui.portable.") else
    "rynui_" + name.removeprefix("rynui.") + "_tests"
    for name in selected if name not in ("rynui.skeleton_docs_example", "rynui.portable.backend_boundaries")
})
output = ROOT / "out" / "083-acceptance"
output.mkdir(parents=True, exist_ok=True)
manifest = {"preset": preset, "count": len(selected), "tests": selected, "targets": targets}
(output / f"{args.configuration}-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
commands = [
    ["cmake", "--build", "--preset", preset, "--target", *targets, "-j", "4"],
    ["ctest", "--preset", preset, "-R", "^(" + "|".join(re.escape(name) for name in selected) + ")$",
     "--parallel", "4", "--output-on-failure"],
]
for index, command in enumerate(commands):
    log = output / f"{args.configuration}-{'build' if index == 0 else 'ctest'}.log"
    print(f"{preset}: {'build' if index == 0 else 'CTest'} {len(targets)} targets / {len(selected)} tests", flush=True)
    result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_bytes(result.stdout)
    tail = result.stdout.decode("utf-8", errors="replace").splitlines()[-12:]
    print("\n".join(tail), flush=True)
    if result.returncode:
        raise SystemExit(f"Failed ({result.returncode}); inspect {log}")
