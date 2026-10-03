"""Exercise explicit SYSTEM package and conformance fixture contracts offline."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--bidi-test", type=Path, required=True)
parser.add_argument("--character-test", type=Path, required=True)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="rynui-bidi-dependency-") as temporary:
    root = Path(temporary)
    for name, version, target, testing, fixtures, mode, expected in [
        ("valid", "3.0.0", True, True, "valid", "SYSTEM", None),
        ("no-tests", "3.0.0", True, False, "none", "SYSTEM", None),
        ("wrong-version", "2.9.0", True, False, "none", "SYSTEM", 'version "3.0.0"'),
        ("missing-target", "3.0.0", False, False, "none", "SYSTEM", "missing required target"),
        ("missing-fixtures", "3.0.0", True, True, "none", "SYSTEM", "require explicit RYNUI_BIDI_TEST_FILE"),
        ("wrong-fixture", "3.0.0", True, True, "bad", "SYSTEM", "fixture hash mismatch"),
        ("bad-mode", "3.0.0", True, False, "none", "AUTO", "explicit BUNDLED or SYSTEM"),
    ]:
        project = root / name
        package = project / "package"
        package.mkdir(parents=True)
        (package / "SheenBidiConfig.cmake").write_text(
            "add_library(SheenBidi::SheenBidi INTERFACE IMPORTED)\n" if target else "", encoding="utf-8")
        (package / "SheenBidiConfigVersion.cmake").write_text(
            f'set(PACKAGE_VERSION "{version}")\n'
            'if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)\n'
            '  set(PACKAGE_VERSION_EXACT TRUE)\n'
            '  set(PACKAGE_VERSION_COMPATIBLE TRUE)\nendif()\n', encoding="utf-8")
        fixture_settings = ""
        if fixtures != "none":
            first = args.bidi_test
            if fixtures == "bad":
                first = project / "wrong.txt"
                first.write_text("wrong Unicode data\n", encoding="utf-8")
            fixture_settings = (f'set(RYNUI_BIDI_TEST_FILE "{first.as_posix()}")\n'
                                f'set(RYNUI_BIDI_CHARACTER_TEST_FILE "{args.character_test.as_posix()}")\n')
        (project / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\nproject(Guard NONE)\n'
            f'set(RYNUI_DEPENDENCY_MODE {mode})\nset(BUILD_TESTING {"ON" if testing else "OFF"})\n'
            f'set(SheenBidi_DIR "{package.as_posix()}")\n{fixture_settings}'
            f'include("{args.source.as_posix()}/cmake/dependencies/RynUISheenBidi.cmake")\n'
            'rynui_resolve_sheenbidi()\n', encoding="utf-8")
        result = subprocess.run(["cmake", "-S", str(project), "-B", str(project / "build"),
                                 "-G", "Ninja Multi-Config"], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if expected is None:
            assert result.returncode == 0, f"{name}: {output}"
        else:
            assert result.returncode != 0 and expected in output, f"{name}: {output}"
print("SheenBidi exact SYSTEM package, target, mode and Unicode fixture contracts passed")
