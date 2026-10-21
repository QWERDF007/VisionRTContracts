#!/usr/bin/env python3
"""
Test isolated installation of VisionRTToolSDK.
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
    if cmake_bin:
        return cmake_bin
    for cand in [
        "E:/Softwares/CMake/bin/cmake.exe",
        "C:/Program Files/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
    ]:
        if Path(cand).is_file():
            return cand
    return "cmake"

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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--opencv-dir", default="")
    parser.add_argument("--opencv-bin", default="")
    parser.add_argument("--make-program", default="")
    parser.add_argument("--cxx-compiler", default="")
    parser.add_argument("--python-interop", action="store_true")
    parser.add_argument("--pybind11-dir", default="")
    parser.add_argument("--python-root", default="")
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

        assert (install_prefix / "include" / "visionrt" / "sdk" / "Value.h").is_file(), "Value.h not installed"
        assert (install_prefix / "lib" / "cmake" / "VisionRTToolSDK" / "VisionRTToolSDKConfig.cmake").is_file(), "Config not installed"

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
            "-G", "Ninja",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            f"-DEXPECT_PYTHON_INTEROP={'ON' if args.python_interop else 'OFF'}",
            f"-DCMAKE_PREFIX_PATH={prefix_arg}"
        ]
        config_cmd.extend([
            f"-DVisionRTToolSDK_DIR={install_prefix / 'lib' / 'cmake' / 'VisionRTToolSDK'}",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
        ])
        if args.make_program:
            config_cmd.append(f"-DCMAKE_MAKE_PROGRAM={args.make_program}")
        if args.cxx_compiler:
            config_cmd.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")
        if args.pybind11_dir:
            config_cmd.append(f"-Dpybind11_DIR={args.pybind11_dir}")
        if args.python_root:
            config_cmd.append(f"-DPython3_ROOT_DIR={args.python_root}")
            config_cmd.append(f"-DPython3_EXECUTABLE={Path(args.python_root) / ('python.exe' if os.name == 'nt' else 'python3')}")

        run_cmd(config_cmd)

        # 4. Build consumer
        print("[Install Test] Building consumer...")
        run_cmd([cmake_bin, "--build", str(consumer_build), "--config", args.config])

        # 5. Execute consumer
        bin_path = consumer_build / "consumer_bin.exe"
        if not bin_path.is_file():
            bin_path = consumer_build / args.config / "consumer_bin.exe"
        assert bin_path.is_file(), f"Consumer binary not found at {bin_path}"

        runtime_paths = ([str(Path(os.environ["SystemRoot"]) / "System32"), os.environ["SystemRoot"]]
                         if os.name == "nt" else os.get_exec_path())
        extra_env = {"PYTHONPATH": ""}
        if args.opencv_bin and Path(args.opencv_bin).is_dir():
            runtime_paths.insert(0, str(Path(args.opencv_bin).resolve()))
        else:
            for p in prefix_paths:
                p_obj = Path(p)
                for bin_cand in [p_obj / "bin", p_obj.parent / "bin", p_obj]:
                    if (bin_cand / "opencv_world480.dll").is_file():
                        runtime_paths.insert(0, str(bin_cand))
                        break

        if args.python_interop and args.python_root:
            extra_env["PYTHONHOME"] = args.python_root
            runtime_paths.insert(0, args.python_root)
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
