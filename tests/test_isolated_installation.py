#!/usr/bin/env python3
"""
Test isolated installation of VisionRTContracts.
Installs SDK to a temporary directory, then builds and runs a standalone consumer project
using CMAKE_PREFIX_PATH, verifying clean decoupled packaging.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

def find_cmake():
    cmake_bin = shutil.which("cmake")
    if not cmake_bin:
        raise RuntimeError("cmake is not available on PATH")
    return cmake_bin

def run_cmd(args, cwd=None, env=None):
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    res = subprocess.run(args, cwd=cwd, env=merged_env, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if res.returncode != 0:
        print(f"Command failed ({res.returncode}): {' '.join(str(a) for a in args)}", file=sys.stderr)
        print("STDOUT:\n", res.stdout, file=sys.stderr)
        print("STDERR:\n", res.stderr, file=sys.stderr)
        sys.exit(res.returncode)
    return res

def dependency_report(binary, cxx_compiler):
    if os.name == "nt":
        candidates = []
        if cxx_compiler:
            candidates.append(Path(cxx_compiler).with_name("dumpbin.exe"))
        found = shutil.which("dumpbin")
        if found:
            candidates.append(Path(found))
        for candidate in candidates:
            if candidate.is_file():
                return run_cmd([str(candidate), "/dependents", str(binary)]).stdout
        print("[Install Test] dumpbin unavailable; dependency check skipped")
        return ""
    if sys.platform == "darwin":
        return run_cmd(["otool", "-L", str(binary)]).stdout
    return run_cmd(["ldd", str(binary)]).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--opencv-dir", default="")
    parser.add_argument("--opencv-bin", default="")
    parser.add_argument("--make-program", default="")
    parser.add_argument("--cxx-compiler", default="")
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--generator-platform", default="")
    args = parser.parse_args()

    cmake_bin = find_cmake()
    build_dir = Path(args.build_dir).resolve()
    temp_dir_obj = tempfile.TemporaryDirectory(prefix="vrt_install_test_")
    temp_dir = Path(temp_dir_obj.name).resolve()

    try:
        install_prefix = temp_dir / "installed_sdk"
        consumer_src = Path(__file__).resolve().parent / "installed_consumer"
        consumer_build = temp_dir / "consumer_build"

        # 1. Install SDK
        print(f"[Install Test] Installing to {install_prefix}...")
        run_cmd([cmake_bin, "--install", str(build_dir), "--prefix", str(install_prefix), "--config", args.config])

        assert (install_prefix / "include" / "visionrt" / "contracts" / "Value.h").is_file(), "Value.h not installed"
        for host_header in ("Schema.h", "Contract.h", "SchemaYaml.h", "ValueYaml.h", "Package.h", "PythonInterop.h"):
            assert not (install_prefix / "include" / "visionrt" / "contracts" / host_header).exists(), f"Host header exported: {host_header}"
        assert not (install_prefix / "bin" / "visionrt_validate_tool.exe").exists(), "Host validator exported by Contracts"
        assert (install_prefix / "lib" / "cmake" / "VisionRTContracts" / "VisionRTContractsConfig.cmake").is_file(), "Config not installed"

        # 3. Configure consumer with CMAKE_PREFIX_PATH
        prefix_paths = [str(install_prefix)]
        if args.opencv_dir:
            prefix_paths.append(args.opencv_dir)
        else:
            cache_file = build_dir / "CMakeCache.txt"
            if cache_file.is_file():
                for line in cache_file.read_text(encoding="utf-8", errors="ignore").splitlines():
                    if line.startswith("OpenCV_DIR:PATH="):
                        prefix_paths.append(line.split("=", 1)[1].strip())
                        break

        prefix_arg = ";".join(prefix_paths)
        print(f"[Install Test] Configuring consumer with CMAKE_PREFIX_PATH={prefix_arg}...")

        config_cmd = [
            cmake_bin,
            "-B", str(consumer_build),
            "-S", str(consumer_src),
            "-G", args.generator,
            f"-DCMAKE_PREFIX_PATH={prefix_arg}"
        ]
        if args.generator_platform:
            config_cmd.extend(["-A", args.generator_platform])
        if not args.generator.startswith("Visual Studio"):
            config_cmd.append(f"-DCMAKE_BUILD_TYPE={args.config}")
        config_cmd.extend([
            f"-DVisionRTContracts_DIR={install_prefix / 'lib' / 'cmake' / 'VisionRTContracts'}",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
        ])
        if not args.generator.startswith("Visual Studio"):
            if args.make_program:
                config_cmd.append(f"-DCMAKE_MAKE_PROGRAM={args.make_program}")
            if args.cxx_compiler:
                config_cmd.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")

        run_cmd(config_cmd)

        # 4. Build consumer
        print("[Install Test] Building consumer...")
        run_cmd([cmake_bin, "--build", str(consumer_build), "--config", args.config])

        # 5. Execute consumer
        binary_name = "consumer_bin.exe" if os.name == "nt" else "consumer_bin"
        bin_path = consumer_build / binary_name
        if not bin_path.is_file():
            bin_path = consumer_build / args.config / binary_name
        assert bin_path.is_file(), f"Consumer binary not found at {bin_path}"

        print("[Install Test] Checking consumer dependencies...")
        report = dependency_report(bin_path, args.cxx_compiler)
        if report:
            forbidden = ("qt6", "qt5", "yaml", "python", "cudart", "inferrt", "nvinfer", "tensorrt")
            lowered = report.lower()
            leaked = [name for name in forbidden if name in lowered]
            assert not leaked, f"Installed consumer leaked host dependencies: {leaked}"

        runtime_paths = ([str(Path(os.environ["SystemRoot"]) / "System32"), os.environ["SystemRoot"]]
                         if os.name == "nt" else os.get_exec_path())
        extra_env = {"PYTHONPATH": ""}
        if args.opencv_bin and Path(args.opencv_bin).is_dir():
            runtime_paths.insert(0, str(Path(args.opencv_bin).resolve()))

        extra_env["PATH"] = os.pathsep.join(runtime_paths)
        print("[Install Test] Running consumer binary...")
        res = run_cmd([str(bin_path)], env=extra_env)
        print(res.stdout)
        print("[Install Test] Isolated installation verified successfully!")
        return 0

    finally:
        temp_dir_obj.cleanup()

if __name__ == "__main__":
    sys.exit(main())
