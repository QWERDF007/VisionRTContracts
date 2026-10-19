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
        consumer_src = temp_dir / "consumer_src"
        consumer_build = temp_dir / "consumer_build"

        # 1. Install SDK
        print(f"[Install Test] Installing to {install_prefix}...")
        run_cmd([cmake_bin, "--install", str(build_dir), "--prefix", str(install_prefix), "--config", args.config])

        assert (install_prefix / "include" / "visionrt" / "sdk" / "Value.h").is_file(), "Value.h not installed"
        assert (install_prefix / "lib" / "cmake" / "VisionRTToolSDK" / "VisionRTToolSDKConfig.cmake").is_file(), "Config not installed"

        # 2. Setup standalone consumer project
        consumer_src.mkdir(parents=True, exist_ok=True)
        cmakelists_content = """cmake_minimum_required(VERSION 3.25)
project(StandaloneConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VisionRTToolSDK CONFIG REQUIRED)

add_executable(consumer_bin main.cpp)
target_link_libraries(consumer_bin PRIVATE VisionRT::ToolSDK)
if(MSVC)
    target_compile_options(consumer_bin PRIVATE /UNDEBUG)
else()
    target_compile_options(consumer_bin PRIVATE -UNDEBUG)
endif()

# Also verify imported executable target exists
if(NOT TARGET VisionRT::visionrt_validate_tool)
    message(FATAL_ERROR "Imported target VisionRT::visionrt_validate_tool not found!")
endif()
"""
        if args.python_interop:
            cmakelists_content += "\ntarget_link_libraries(consumer_bin PRIVATE VisionRT::PythonInterop)\n"
        else:
            cmakelists_content += "\nif(TARGET VisionRT::PythonInterop)\nmessage(FATAL_ERROR \"Core-only SDK unexpectedly exports PythonInterop\")\nendif()\n"
        (consumer_src / "CMakeLists.txt").write_text(cmakelists_content, encoding="utf-8")

        main_cpp_content = """#include <visionrt/sdk/Value.h>
#include <visionrt/sdk/Schema.h>
#include <iostream>

int main() {
    using namespace visionrt;
    PortValue val{std::int64_t{42}};
    if (std::holds_alternative<std::int64_t>(val) && std::get<std::int64_t>(val) == 42) {
        std::cout << "[Standalone Consumer] Successfully verified installed VisionRTToolSDK!" << std::endl;
        return 0;
    }
    return 1;
}
"""
        if args.python_interop:
            main_cpp_content = """#include <visionrt/sdk/PythonInterop.h>
#include <visionrt/sdk/Schema.h>
#include <pybind11/embed.h>
#include <iostream>

int main() {
    visionrt::PortValue result;
    {
        pybind11::scoped_interpreter interpreter;
        auto scope = pybind11::dict();
        {
            auto image = cv::Mat(3, 4, CV_8UC1, cv::Scalar(17));
            visionrt::PortValue input = visionrt::ImageBatch({visionrt::OwnedImage::copyOf(image)});
            scope["images"] = visionrt::toPythonReadonly(input);
        }
        pybind11::exec("assert images[0].shape == (3, 4)\\nassert not images[0].flags.writeable\\nassert int(images[0][2, 3]) == 17", scope);
        result = visionrt::fromPython({visionrt::Type::ImageBatch}, scope["images"]);
        scope["labels"] = visionrt::toPythonReadonly(visionrt::StringArray(1, visionrt::Layout::Dense,
            {visionrt::StringBuffer({2}, {"cat", "dog"})}));
        pybind11::exec("assert labels.tolist() == ['cat', 'dog']\\nassert not labels.flags.writeable", scope);
    }
    const auto& image = std::get<visionrt::ImageBatch>(result).images()[0];
    if (image.rows() != 3 || image.cols() != 4 || image.data()[0] != 17) return 1;
    std::cout << "[Standalone Consumer] Installed PythonInterop and C++ ownership verified!" << std::endl;
    return 0;
}
"""
        (consumer_src / "main.cpp").write_text(main_cpp_content, encoding="utf-8")

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
            f"-DCMAKE_PREFIX_PATH={prefix_arg}"
        ]
        if args.make_program:
            config_cmd.append(f"-DCMAKE_MAKE_PROGRAM={args.make_program}")
        if args.cxx_compiler:
            config_cmd.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")
        if args.pybind11_dir:
            config_cmd.append(f"-Dpybind11_DIR={args.pybind11_dir}")
        if args.python_root:
            config_cmd.append(f"-DPython3_ROOT_DIR={args.python_root}")

        run_cmd(config_cmd)

        # 4. Build consumer
        print("[Install Test] Building consumer...")
        run_cmd([cmake_bin, "--build", str(consumer_build), "--config", args.config])

        # 5. Execute consumer
        bin_path = consumer_build / "consumer_bin.exe"
        if not bin_path.is_file():
            bin_path = consumer_build / args.config / "consumer_bin.exe"
        assert bin_path.is_file(), f"Consumer binary not found at {bin_path}"

        extra_env = {}
        if args.opencv_bin and Path(args.opencv_bin).is_dir():
            extra_env["PATH"] = str(Path(args.opencv_bin).resolve()) + os.pathsep + os.environ.get("PATH", "")
        else:
            for p in prefix_paths:
                p_obj = Path(p)
                for bin_cand in [p_obj / "bin", p_obj.parent / "bin", p_obj]:
                    if (bin_cand / "opencv_world480.dll").is_file():
                        extra_env["PATH"] = str(bin_cand) + os.pathsep + os.environ.get("PATH", "")
                        break

        if args.python_interop and args.python_root:
            extra_env["PYTHONHOME"] = args.python_root
            extra_env["PATH"] = args.python_root + os.pathsep + extra_env.get("PATH", os.environ.get("PATH", ""))
        print("[Install Test] Running consumer binary...")
        res = run_cmd([str(bin_path)], env=extra_env)
        print(res.stdout)
        print("[Install Test] Isolated installation verified successfully!")
        return 0

    finally:
        temp_dir_obj.cleanup()

if __name__ == "__main__":
    sys.exit(main())
