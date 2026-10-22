cmake_minimum_required(VERSION 3.25)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(EXISTS "${OpenCV_DIR}/../bin")
    file(REAL_PATH "${OpenCV_DIR}/../bin" _opencv_bin_dir)
elseif(EXISTS "${OpenCV_DIR}/bin")
    file(REAL_PATH "${OpenCV_DIR}/bin" _opencv_bin_dir)
else()
    set(_opencv_bin_dir "")
endif()

if(TARGET VisionRTContractsPython)
    add_executable(test_rect_interop test_rect_interop.cpp)
    target_link_libraries(test_rect_interop PRIVATE VisionRT::ContractsPython)
    add_test(NAME test_rect_interop COMMAND test_rect_interop)
    get_filename_component(_rect_python_root "${Python3_EXECUTABLE}" DIRECTORY)
    set_tests_properties(test_rect_interop PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_opencv_bin_dir};PATH=path_list_prepend:${_rect_python_root}")
endif()

# 1. test_sdk_value_contract
add_executable(test_sdk_value_contract test_sdk_value_contract.cpp)
target_link_libraries(test_sdk_value_contract PRIVATE VisionRT::Contracts)
if(MSVC)
    target_compile_options(test_sdk_value_contract PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_value_contract COMMAND test_sdk_value_contract)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_value_contract PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 2. test_sdk_consumer
add_executable(test_sdk_consumer test_sdk_consumer.cpp)
target_link_libraries(test_sdk_consumer PRIVATE VisionRT::Contracts)
if(MSVC)
    target_compile_options(test_sdk_consumer PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_consumer COMMAND test_sdk_consumer)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_consumer PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 3. test_sdk_runtime_contract
add_executable(test_sdk_runtime_contract test_sdk_runtime_contract.cpp)
target_link_libraries(test_sdk_runtime_contract PRIVATE VisionRT::Contracts)
if(MSVC)
    target_compile_options(test_sdk_runtime_contract PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_runtime_contract COMMAND test_sdk_runtime_contract)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_runtime_contract PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 4. test_sdk_package_contract
add_executable(test_sdk_package_contract test_sdk_package_contract.cpp)
target_link_libraries(test_sdk_package_contract PRIVATE VisionRT::Contracts)
if(MSVC)
    target_compile_options(test_sdk_package_contract PRIVATE /utf-8)
endif()
add_test(NAME test_sdk_package_contract COMMAND test_sdk_package_contract)
if(_opencv_bin_dir)
    set_tests_properties(test_sdk_package_contract PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 4. test_cli
add_test(
    NAME test_cli
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py"
            --validator "$<TARGET_FILE:visionrt_validate_tool>"
            --opencv-bin "${_opencv_bin_dir}"
)
if(_opencv_bin_dir)
    set_tests_properties(test_cli PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

# 5. test_isolated_installation
set(_interop_install_args "")
if(VISIONRT_BUILD_PYTHON_INTEROP)
    get_filename_component(_python_root "${Python3_EXECUTABLE}" DIRECTORY)
    list(APPEND _interop_install_args --python-interop --pybind11-dir "${pybind11_DIR}" --python-root "${_python_root}")
endif()
add_test(
    NAME test_isolated_installation
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_isolated_installation.py"
            --build-dir "${CMAKE_BINARY_DIR}"
            --config "$<CONFIG>"
            --opencv-dir "${OpenCV_DIR}"
            --opencv-bin "${_opencv_bin_dir}"
            --make-program "${CMAKE_MAKE_PROGRAM}"
            --cxx-compiler "${CMAKE_CXX_COMPILER}"
            ${_interop_install_args}
)
if(_opencv_bin_dir)
    set_tests_properties(test_isolated_installation PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_opencv_bin_dir}")
endif()

foreach(_sdk_test IN ITEMS test_sdk_value_contract test_sdk_consumer test_sdk_package_contract)
    if(MSVC)
        target_compile_options(${_sdk_test} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${_sdk_test} PRIVATE -UNDEBUG)
    endif()
endforeach()
