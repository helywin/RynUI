import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
WRITER_BINARY = sys.argv.pop(1) if len(sys.argv) > 1 else None
spec = importlib.util.spec_from_file_location("gallery_runner", ROOT / "tools/run_gallery_page_acceptance.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def result():
    return dict(schema_version=1, platform="linux", preset="linux-gcc", configuration="Release", toolchain="GNU",
                backend="vulkan", shader="SPIRV", font_source="system", error="", system_fonts=True,
                host_input_area=True, host_cursor_sync=True, checks=24, submitted_frames=12, cases=11, passed=True)


class RunnerTests(unittest.TestCase):
    @unittest.skipUnless(WRITER_BINARY, "CMake supplies the actual C++ writer fixture")
    def test_cpp_result_writer_json(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="page-writer-test-") as temp:
            output = Path(temp) / "result.json"
            completed = subprocess.run([WRITER_BINARY, str(output)], capture_output=True, text=True, timeout=10)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            value = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(value["error"], 'fixture line\n"quote"\t\\')
            self.assertIs(value["passed"], False)
            self.assertEqual(value["schema_version"], 1)

    def test_schema_and_cross_platform_evidence(self):
        self.assertEqual(runner.validate_result(result(), "linux", "linux-gcc", "Release")["cases"], 11)
        for patch in ({"checks": True}, {"platform": "windows"}, {"submitted_frames": 0}, {"passed": False},
                      {"error": "GPU failed"}, {"backend": "recording"}, {"system_fonts": False}, {"schema_version": 2}):
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                runner.validate_result(result() | patch, "linux", "linux-gcc", "Release")
        with self.assertRaises(ValueError):
            runner.validate_result(result() | {"unexpected": 1}, "linux", "linux-gcc", "Release")
        with self.assertRaises(ValueError):
            runner.validate_result(result() | {"platform": "windows", "preset": "windows-msvc", "backend": "d3d12"},
                                   "windows", "windows-msvc", "Release")

    def test_linux_private_wayland_command(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="page-runner-test-") as temp:
            directory = Path(temp)
            def execute(command, **kwargs):
                self.assertEqual(Path(command[1]).name, "run_background_wayland.py")
                self.assertIn("--", command)
                self.assertEqual(kwargs["timeout"], 25)
                output = next(item.split("=", 1)[1] for item in command if item.startswith("--page-output="))
                Path(output).write_text(json.dumps(result()), encoding="utf-8")
                return types.SimpleNamespace(returncode=0, stdout="contract fixture\n", stderr="")
            self.assertEqual(runner.run(sys.executable, directory, "linux-gcc", timeout=5, execute=execute, system="Linux"), 0)
            summary = json.loads((directory / "runner-result.json").read_text())
            self.assertEqual(summary["status"], "passed")
            self.assertFalse(summary["desktop_fallback"])

    def test_exit_timeout_missing_result_and_stale_output(self):
        for code in (0, 3, 124):
            with self.subTest(code=code), tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="page-runner-test-") as temp:
                directory = Path(temp)
                (directory / "native-result.json").write_text(json.dumps(result()), encoding="utf-8")
                def execute(command, **kwargs):
                    return types.SimpleNamespace(returncode=code, stdout="", stderr="fixture failure")
                self.assertEqual(runner.run(sys.executable, directory, "linux-gcc", execute=execute, system="Linux"), 1)
                self.assertFalse((directory / "native-result.json").exists())
                summary = json.loads((directory / "runner-result.json").read_text())
                self.assertEqual(summary["status"], "timeout" if code == 124 else "failed")
        with tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="page-runner-test-") as temp:
            def expired(command, **kwargs):
                raise subprocess.TimeoutExpired(command, 1)
            self.assertEqual(runner.run(sys.executable, temp, "linux-gcc", execute=expired, system="Linux"), 1)
            self.assertEqual(json.loads((Path(temp) / "runner-result.json").read_text())["status"], "timeout")

    def test_output_boundary(self):
        with self.assertRaises(ValueError):
            runner.run(sys.executable, ROOT / "docs", "linux-gcc", execute=lambda *_: self.fail("must reject before execution"))
        with tempfile.TemporaryDirectory(dir=ROOT / "out", prefix="page-runner-test-") as temp:
            with self.assertRaises(ValueError):
                runner.run(sys.executable, temp, "linux-gcc", timeout=0)


if __name__ == "__main__":
    unittest.main()
