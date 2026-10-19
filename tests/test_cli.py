#!/usr/bin/env python3
"""
CLI contract tests for visionrt_validate_tool.
Verifies tool yaml, --index, and --package modes against valid and negative cases,
including paths containing spaces and Chinese characters.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

def run_cmd(args, env=None):
    return subprocess.run(args, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--validator", required=True, help="Path to visionrt_validate_tool binary")
    parser.add_argument("--opencv-bin", default="", help="Path to OpenCV bin directory")
    args = parser.parse_args()

    validator_bin = Path(args.validator).resolve()
    if not validator_bin.is_file():
        print(f"Validator binary not found: {validator_bin}", file=sys.stderr)
        return 1

    test_env = os.environ.copy()
    if args.opencv_bin and Path(args.opencv_bin).is_dir():
        test_env["PATH"] = str(Path(args.opencv_bin).resolve()) + os.pathsep + test_env["PATH"]

    temp_dir_obj = tempfile.TemporaryDirectory(prefix="vrt_cli_test_")
    base_dir = Path(temp_dir_obj.name).resolve()

    try:
        # 1. Single tool validation
        valid_tool_yaml = base_dir / "valid_tool.yaml"
        valid_tool_yaml.write_text(
            """schema_version: 1
tool_id: test.cli.pass
display_name: CLI Pass Tool
version: 1.0.0
kind: cpp_function
entry:
  registry_name: test.cli.pass
init_params: []
inputs:
  - id: count
    name: 数量
    type: Int
    default: 1
outputs: []
""",
            encoding="utf-8"
        )
        res = run_cmd([str(validator_bin), str(valid_tool_yaml)], env=test_env)
        assert res.returncode == 0, f"Expected 0 for valid tool yaml, got {res.returncode}. Output:\n{res.stdout}\n{res.stderr}"

        # 1b. Invalid tool validation
        invalid_tool_yaml = base_dir / "invalid_tool.yaml"
        invalid_tool_yaml.write_text(
            """schema_version: 2
tool_id: test.cli.fail
""",
            encoding="utf-8"
        )
        res = run_cmd([str(validator_bin), str(invalid_tool_yaml)], env=test_env)
        assert res.returncode != 0, f"Expected non-zero for invalid schema_version, got {res.returncode}"

        # 2. Module index validation (--index)
        valid_index_yaml = base_dir / "module_index.yaml"
        valid_index_yaml.write_text(
            """schema_version: 2
variant: cpu
modules:
  mod_test:
    library: bin/mod_test.dll
tools:
  test.cli.pass:
    module: mod_test
    definition: tools/tool.yaml
runtime_files: []
""",
            encoding="utf-8"
        )
        res = run_cmd([str(validator_bin), "--index", str(valid_index_yaml)], env=test_env)
        assert res.returncode == 0, f"Expected 0 for valid index, got {res.returncode}. Output:\n{res.stdout}\n{res.stderr}"

        # 2b. Negative index: dangling module
        dangling_index_yaml = base_dir / "dangling_index.yaml"
        dangling_index_yaml.write_text(
            """schema_version: 2
variant: cpu
modules:
  mod_test:
    library: bin/mod_test.dll
tools:
  test.cli.pass:
    module: nonexistent_mod
    definition: tools/tool.yaml
""",
            encoding="utf-8"
        )
        res = run_cmd([str(validator_bin), "--index", str(dangling_index_yaml)], env=test_env)
        assert res.returncode != 0, f"Expected non-zero for dangling module index, got {res.returncode}"

        # 2c. Negative index: duplicate key
        dup_key_yaml = base_dir / "dup_key_index.yaml"
        dup_key_yaml.write_text(
            """schema_version: 2
variant: cpu
variant: gpu
modules: {}
tools: {}
""",
            encoding="utf-8"
        )
        res = run_cmd([str(validator_bin), "--index", str(dup_key_yaml)], env=test_env)
        assert res.returncode != 0, f"Expected non-zero for duplicate key index, got {res.returncode}"

        # 3. Package validation (--package) including Chinese and space paths
        pkg_dir = base_dir / "测试 目录" / "工具 包 1.0"
        (pkg_dir / "bin").mkdir(parents=True, exist_ok=True)
        (pkg_dir / "tools").mkdir(parents=True, exist_ok=True)

        (pkg_dir / "bin" / "mod_test.dll").write_bytes(b"dummy dll")
        (pkg_dir / "bin" / "opencv_core.dll").write_bytes(b"dummy opencv")

        pkg_tool_yaml = pkg_dir / "tools" / "tool.yaml"
        pkg_tool_yaml.write_text(
            """schema_version: 1
tool_id: test.pkg.tool
display_name: 中文路径工具
version: 1.0.0
kind: cpp_function
entry:
  registry_name: test.pkg.tool
init_params: []
inputs: []
outputs: []
""",
            encoding="utf-8"
        )

        pkg_index_yaml = pkg_dir / "module_index.yaml"
        pkg_index_yaml.write_text(
            """schema_version: 2
variant: cpu
modules:
  mod_test:
    library: bin/mod_test.dll
tools:
  test.pkg.tool:
    module: mod_test
    definition: tools/tool.yaml
runtime_files:
  - bin/opencv_core.dll
""",
            encoding="utf-8"
        )

        res = run_cmd([str(validator_bin), "--package", str(pkg_dir)], env=test_env)
        assert res.returncode == 0, f"Expected 0 for valid package with Chinese/space path, got {res.returncode}. Output:\n{res.stdout}\n{res.stderr}"

        # 3b. Negative package: missing runtime file
        (pkg_dir / "bin" / "opencv_core.dll").unlink()
        res = run_cmd([str(validator_bin), "--package", str(pkg_dir)], env=test_env)
        assert res.returncode != 0, f"Expected non-zero for missing runtime_files in package, got {res.returncode}"

        # 3c. Negative package: missing definition file
        (pkg_dir / "bin" / "opencv_core.dll").write_bytes(b"dummy")
        pkg_tool_yaml.unlink()
        res = run_cmd([str(validator_bin), "--package", str(pkg_dir)], env=test_env)
        assert res.returncode != 0, f"Expected non-zero for missing definition in package, got {res.returncode}"

        print("[CLI Test] All CLI contract tests passed successfully!")
        return 0

    finally:
        temp_dir_obj.cleanup()

if __name__ == "__main__":
    sys.exit(main())
