#!/usr/bin/env python3
"""Format repository-owned C++ and HLSL with the shared clang-format style."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = {"include", "src", "examples", "tests", "shaders"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hlsl"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check formatting without changing files")
    parser.add_argument("--clang-format", default="clang-format", help="clang-format executable (version 22.x)")
    args = parser.parse_args()
    formatter = shutil.which(args.clang_format)
    if formatter is None:
        parser.error("clang-format was not found; add it to PATH or pass --clang-format")
    version = subprocess.run(
        [formatter, "--version"], check=True, capture_output=True, text=True, encoding="utf-8"
    ).stdout.strip()
    if not re.search(r"\bversion 22\.", version):
        parser.error(f"Use clang-format 22.x for reproducible formatting; found: {version}")

    tracked = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True
    ).stdout.decode("utf-8").split("\0")
    sources = [
        path for path in tracked
        if Path(path).parts and Path(path).parts[0] in SOURCE_ROOTS
        and Path(path).suffix in SOURCE_SUFFIXES and not path.startswith("tests/fixtures/")
    ]
    flags = ["--style=file", "--fallback-style=none"]
    flags += ["--dry-run", "--Werror"] if args.check else ["-i"]

    def run_formatter(path: str) -> tuple[str, subprocess.CompletedProcess]:
        return path, subprocess.run(
            [formatter, *flags, path], cwd=ROOT, capture_output=True, text=True, encoding="utf-8"
        )

    failures = 0
    with ThreadPoolExecutor(max_workers=8) as pool:
        for path, result in pool.map(run_formatter, sources):
            if result.returncode:
                failures += 1
                print(f"{path}: {result.stderr.strip()}", file=sys.stderr)
    action = "Checked" if args.check else "Formatted"
    print(f"{action} {len(sources)} owned source files with {version}; {failures} failures.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
